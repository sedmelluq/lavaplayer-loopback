package com.sedmelluq.lavaplayer.loopback.natives;

import com.sedmelluq.discord.lavaplayer.natives.NativeLibLoader;

import java.nio.ByteBuffer;
import java.nio.ShortBuffer;

class AudioLoopbackLibrary {
  private AudioLoopbackLibrary() {

  }

  static AudioLoopbackLibrary getInstance() {
    NativeLibLoader.load(AudioLoopbackLibrary.class, "loopback");
    return new AudioLoopbackLibrary();
  }

  native long create();

  native long initialise(long instance, ByteBuffer formatDirectBuffer, String deviceName);

  native long read(long instance, ShortBuffer sampleDirectBuffer, int capacity);

  native void destroy(long instance);
}
