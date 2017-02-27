package com.sedmelluq.lavaplayer.loopback.natives;

import com.sedmelluq.discord.lavaplayer.natives.NativeResourceHolder;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.ShortBuffer;

public class AudioLoopback extends NativeResourceHolder {
  private static final Logger log = LoggerFactory.getLogger(AudioLoopback.class);

  private static final long INVALIDATED_ERROR = Long.parseUnsignedLong("8000001388890004", 16);

  private final AudioLoopbackLibrary library;
  private final long instance;

  public AudioLoopback() {
    library = AudioLoopbackLibrary.getInstance();
    instance = library.create();
  }

  public Format initialise() {
    ByteBuffer formatBuffer = ByteBuffer.allocateDirect(8).order(ByteOrder.nativeOrder());

    long result = library.initialise(instance, formatBuffer);
    if (result < 0) {
      throw new IllegalStateException("Initialising loopback failed with error " + result);
    }

    return new Format(formatBuffer.getInt(0), formatBuffer.getInt(4));
  }

  public int read(ShortBuffer buffer) {
    checkNotReleased();

    long result = library.read(instance, buffer, buffer.capacity());

    if (result < 0) {
      if (result == INVALIDATED_ERROR) {
        log.debug("Device has been invalidated, emitting EOF.");
        return -1;
      }

      throw new IllegalStateException("Reading from loopback failed with error " + result);
    } else {
      int samples = (int) result;
      buffer.clear();
      buffer.limit(samples);
      return samples;
    }
  }

  @Override
  protected void freeResources() {
    library.destroy(instance);
  }

  public static class Format {
    public final int channelCount;
    public final int sampleRate;

    public Format(int channelCount, int sampleRate) {
      this.channelCount = channelCount;
      this.sampleRate = sampleRate;
    }
  }
}
