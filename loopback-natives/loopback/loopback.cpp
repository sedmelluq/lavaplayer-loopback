#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <jni.h>
#include <stdint.h>
#include <Functiondiscoverykeys_devpkey.h>

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

enum loopback_error_e {
	error_none,
	error_co_initialize,
	error_acquire_enumerator,
	error_acquire_device,
	error_acquire_client,
	error_get_mix_format,
	error_initialize_client,
	error_get_buffer_size,
	error_unsupported_format_tag,
	error_unsupported_format_subtype,
	error_acquire_capture,
	error_client_start,
	error_null,
	error_already_initialised,
	error_not_initialised,
	error_invalid_buffer,
	error_invalid_buffer_capacity,
	error_invalid_block_align,
	error_invalid_buffer_size,
	error_capture_buffer,
	error_enumerate_devices,
	error_enumerate_count,
	error_enumerate_item,
	error_enumerate_property,
	error_enumerate_name,
	error_no_match
};

struct loopback_state_t {
	IMMDevice* device;
	IAudioClient* audio_client;
	IAudioCaptureClient* capture_client;
	WAVEFORMATEX* format;
	bool initialisation_started;
	bool initialised;
	uint32_t com_thread_id;
	uint32_t buffer_frame_count;
	size_t packet_offset;
};

struct output_info_t {
	int32_t channel_count;
	int32_t sample_rate;
};

static size_t size_min(size_t a, size_t b) {
	return a < b ? a : b;
}

static int64_t loopback_com_error(loopback_error_e operation, HRESULT result) {
	return (int64_t) (((uint64_t) operation << 32ULL) | (((uint64_t) result) & 0xFFFFFFFF) | 0x8000000000000000ULL);
}

static int64_t loopback_error(loopback_error_e error) {
	return (int64_t) (((uint64_t) error << 32ULL) | 0x8000000000000000ULL);
}

static loopback_state_t* loopback_create(void) {
	return (loopback_state_t*) calloc(1, sizeof(loopback_state_t));
}

static void loopback_release_entity(IUnknown** entity) {
	if (*entity != NULL) {
		(*entity)->Release();
		*entity = NULL;
	}
}

static void loopback_shutdown(loopback_state_t* state) {
	state->initialised = false;

	if (state->audio_client != NULL) {
		state->audio_client->Stop();
	}

	if (state->format != NULL) {
		CoTaskMemFree(state->format);
		state->format = NULL;
	}

	loopback_release_entity((IUnknown**) &state->capture_client);
	loopback_release_entity((IUnknown**) &state->audio_client);
	loopback_release_entity((IUnknown**) &state->device);

	if (state->com_thread_id == GetCurrentThreadId()) {
		CoUninitialize();
		state->com_thread_id = 0;
	}
}

static void loopback_destroy(loopback_state_t* state) {
	if (state != NULL) {
		loopback_shutdown(state);
		free(state);
	}
}

static int64_t loopback_com_initialise(loopback_state_t* state) {
	HRESULT result = CoInitialize(NULL);

	if (FAILED(result)) {
		return loopback_com_error(error_co_initialize, result);
	}
	else if (result == S_OK) {
		state->com_thread_id = GetCurrentThreadId();
	}

	return 0;
}

static int64_t loopback_device_match(IMMDevice* device, const wchar_t* device_name, bool* matched) {
	IPropertyStore* properties;
	HRESULT result = device->OpenPropertyStore(STGM_READ, &properties);

	if (FAILED(result)) {
		return loopback_com_error(error_enumerate_property, result);
	}

	PROPVARIANT name_field;
	PropVariantInit(&name_field);

	result = properties->GetValue(PKEY_Device_FriendlyName, &name_field);
	loopback_release_entity((IUnknown**) &properties);

	if (FAILED(result)) {
		return loopback_com_error(error_enumerate_name, result);
	}

	*matched = wcscmp(device_name, name_field.pwszVal) == 0;
	PropVariantClear(&name_field);

	return error_none;
}

static int64_t loopback_device_find_in_collection(IMMDeviceCollection* collection, const wchar_t* device_name, IMMDevice** device_out) {
	uint32_t count;
	HRESULT result = collection->GetCount(&count);

	if (FAILED(result)) {
		return loopback_com_error(error_enumerate_count, result);
	}

	for (uint32_t index = 0; index < count; index++) {
		IMMDevice* device;
		result = collection->Item(index, &device);

		if (FAILED(result)) {
			return loopback_com_error(error_enumerate_item, result);
		}

		bool matched = false;
		int64_t error = loopback_device_match(device, device_name, &matched);

		if (error == error_none && matched) {
			*device_out = device;
			return error_none;
		}

		loopback_release_entity((IUnknown**) &device);

		if (error != 0) {
			return error;
		}
	}

	return loopback_error(error_no_match);
}

static int64_t loopback_device_find(IMMDeviceEnumerator* enumerator, const wchar_t* device_name, IMMDevice** device_out) {
	IMMDeviceCollection* collection = NULL;
	HRESULT result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);

	if (FAILED(result)) {
		return loopback_com_error(error_enumerate_devices, result);
	}

	int64_t error = loopback_device_find_in_collection(collection, device_name, device_out);
	loopback_release_entity((IUnknown**) &collection);

	return error;
}

static int64_t loopback_device_initialise(loopback_state_t* state, const wchar_t* device_name) {
	IMMDeviceEnumerator* enumerator = NULL;
	HRESULT result = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**) &enumerator);
	int64_t error = 0;

	if (FAILED(result)) {
		return loopback_com_error(error_acquire_enumerator, result);
	}

	if (device_name == NULL) {
		result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &state->device);

		if (FAILED(result)) {
			error = loopback_com_error(error_acquire_device, result);
		}
	}
	else {
		error = loopback_device_find(enumerator, device_name, &state->device);
	}

	loopback_release_entity((IUnknown**) &enumerator);
	return error;
}

static int64_t loopback_verify_known_format(loopback_state_t* state) {
	if (state->format->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
		return loopback_error(error_unsupported_format_tag);
	}

	WAVEFORMATEXTENSIBLE* extensible = (WAVEFORMATEXTENSIBLE*) state->format;

	if (memcmp(&extensible->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) != 0) {
		return loopback_error(error_unsupported_format_subtype);
	}
	else if (state->format->nBlockAlign != sizeof(float) * state->format->nChannels) {
		return loopback_error(error_invalid_block_align);
	}

	return 0;
}

static int64_t loopback_client_initialise(loopback_state_t* state) {
	HRESULT result;
	int64_t error;

	if (FAILED(result = state->device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**) &state->audio_client))) {
		return loopback_com_error(error_acquire_client, result);
	}
	else if (FAILED(result = state->audio_client->GetMixFormat(&state->format))) {
		return loopback_com_error(error_get_mix_format, result);
	}

	if ((error = loopback_verify_known_format(state)) != 0) {
		return error;
	}

	if (FAILED(result = state->audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, REFTIMES_PER_SEC, 0, state->format, NULL))) {
		return loopback_com_error(error_initialize_client, result);
	}
	else if (FAILED(result = state->audio_client->GetBufferSize(&state->buffer_frame_count))) {
		return loopback_com_error(error_get_buffer_size, result);
	}

	return 0;
}


static int64_t loopback_capture_initialise(loopback_state_t* state) {
	HRESULT result;

	if (FAILED(result = state->audio_client->GetService(IID_IAudioCaptureClient, (void**) &state->capture_client))) {
		return loopback_com_error(error_acquire_capture, result);
	}
	else if (FAILED(result = state->audio_client->Start())) {
		return loopback_com_error(error_client_start, result);
	}

	return 0;
}

static int64_t loopback_initialise(loopback_state_t* state, output_info_t* output_info, const wchar_t* device_name) {
	if (state == NULL) {
		return loopback_error(error_null);
	}
	else if (state->initialisation_started) {
		return loopback_error(error_already_initialised);
	}

	state->initialisation_started = true;

	int64_t error = 0;

	if ((error = loopback_com_initialise(state)) != 0 ||
		(error = loopback_device_initialise(state, device_name)) != 0 ||
		(error = loopback_client_initialise(state)) != 0 ||
		(error = loopback_capture_initialise(state)) != 0) {

		loopback_shutdown(state);
	}
	else {
		state->initialised = true;
		output_info->channel_count = state->format->nChannels;
		output_info->sample_rate = state->format->nSamplesPerSec;
	}

	return error;
}

static int16_t loopback_convert(float sample) {
	int32_t value = (int32_t) (sample * 32768.0f);

	if (value < -32768) {
		return -32768;
	}
	else if (value > 32767) {
		return 32767;
	}
	else {
		return (int16_t) value;
	}
}

static int64_t loopback_read(loopback_state_t* state, int16_t* s16le_data, size_t capacity) {
	if (state == NULL) {
		return loopback_error(error_null);
	}

	size_t out_offset = 0;

	if (capacity % state->format->nChannels != 0) {
		return loopback_error(error_invalid_buffer_size);
	}

	while (out_offset < capacity) {
		float* data;
		uint32_t frame_count;
		uint32_t flags;
		HRESULT result = state->capture_client->GetBuffer((BYTE**) &data, &frame_count, (DWORD*) &flags, NULL, NULL);

		if (FAILED(result)) {
			return loopback_com_error(error_capture_buffer, result);
		}
		else if (result == AUDCLNT_S_BUFFER_EMPTY) {
			state->capture_client->ReleaseBuffer(frame_count);
			break;
		}

		size_t copy_frames = size_min((capacity - out_offset) / state->format->nChannels, frame_count - state->packet_offset);
		size_t copy_samples = copy_frames * state->format->nChannels;
		size_t offset = state->packet_offset * state->format->nChannels;

		for (size_t i = 0; i < copy_samples; i++) {
			s16le_data[out_offset + i] = loopback_convert(data[offset + i]);
		}

		state->packet_offset += copy_frames;
		out_offset += copy_samples;

		if (state->packet_offset < frame_count) {
			state->capture_client->ReleaseBuffer(0);
		}
		else {
			state->packet_offset = 0;
			state->capture_client->ReleaseBuffer(frame_count);
		}
	}

	return (int64_t) out_offset;
}

extern "C" JNIEXPORT jlong JNICALL Java_com_sedmelluq_lavaplayer_loopback_natives_AudioLoopbackLibrary_create(JNIEnv* jni, jobject me) {
	return (jlong) loopback_create();
}

extern "C" JNIEXPORT jlong JNICALL Java_com_sedmelluq_lavaplayer_loopback_natives_AudioLoopbackLibrary_initialise(JNIEnv* jni, jobject me, jlong instance, jobject format_buffer, jstring name_string) {
	const jchar* device_name = NULL;

	if (name_string != NULL) {
		device_name = jni->GetStringChars(name_string, NULL);
	}

	if (jni->GetDirectBufferCapacity(format_buffer) < sizeof(output_info_t)) {
		return loopback_error(error_invalid_buffer_capacity);
	}

	output_info_t* format = (output_info_t*) jni->GetDirectBufferAddress(format_buffer);

	if (format == NULL) {
		return loopback_error(error_invalid_buffer);
	}

	jlong result = (jlong) loopback_initialise((loopback_state_t*) instance, format, (const wchar_t*) device_name);

	if (device_name != NULL) {
		jni->ReleaseStringChars(name_string, device_name);
	}

	return result;
}

extern "C" JNIEXPORT jlong JNICALL Java_com_sedmelluq_lavaplayer_loopback_natives_AudioLoopbackLibrary_read(JNIEnv* jni, jobject me, jlong instance, jobject s16le_buffer, jint capacity) {
	int16_t* s16le_data = (int16_t*) jni->GetDirectBufferAddress(s16le_buffer);

	if (s16le_data == NULL) {
		return loopback_error(error_invalid_buffer);
	}

	return (jlong) loopback_read((loopback_state_t*) instance, s16le_data, capacity);
}

extern "C" JNIEXPORT void JNICALL Java_com_sedmelluq_lavaplayer_loopback_natives_AudioLoopbackLibrary_destroy(JNIEnv* jni, jobject me, jlong instance) {
	loopback_destroy((loopback_state_t*) instance);
}
