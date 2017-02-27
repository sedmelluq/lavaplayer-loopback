package com.sedmelluq.lavaplayer.loopback;

import com.sedmelluq.discord.lavaplayer.filter.FilterChainBuilder;
import com.sedmelluq.discord.lavaplayer.filter.ShortPcmAudioFilter;
import com.sedmelluq.discord.lavaplayer.track.playback.AudioProcessingContext;
import com.sedmelluq.lavaplayer.loopback.natives.AudioLoopback;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.ShortBuffer;

public class LoopbackFrameProvider {
  private final AudioLoopback loopback;
  private final ShortPcmAudioFilter downstream;
  private final ShortBuffer buffer;
  private long lastExhausted = 0;

  public LoopbackFrameProvider(AudioLoopback loopback, AudioLoopback.Format format, AudioProcessingContext context) {
    this.loopback = loopback;
    this.buffer = ByteBuffer.allocateDirect(4096 * format.channelCount * 2).order(ByteOrder.nativeOrder()).asShortBuffer();
    this.downstream = FilterChainBuilder.forShortPcm(context, format.channelCount, format.sampleRate, true);
  }

  public void close() {
    downstream.close();
  }

  public void provideFrames() throws InterruptedException {
    while (true) {
      waitForData();

      int samples = loopback.read(buffer);
      if (samples < 0) {
        break;
      }

      if (samples < buffer.capacity()) {
        lastExhausted = System.nanoTime();
      }

      if (samples > 0) {
        downstream.process(buffer);
      }
    }
  }

  public void waitForData() throws InterruptedException {
    int waitTime =  (int) (Math.max(lastExhausted + 20_000_000L - System.nanoTime(), 0) / 1_000_000L);

    if (waitTime > 0) {
      Thread.sleep(waitTime);
    }
  }
}
