package com.sedmelluq.lavaplayer.loopback;

import com.sedmelluq.discord.lavaplayer.source.AudioSourceManager;
import com.sedmelluq.discord.lavaplayer.track.AudioTrack;
import com.sedmelluq.discord.lavaplayer.track.AudioTrackInfo;
import com.sedmelluq.discord.lavaplayer.track.BaseAudioTrack;
import com.sedmelluq.discord.lavaplayer.track.playback.LocalAudioTrackExecutor;
import com.sedmelluq.lavaplayer.loopback.natives.AudioLoopback;

public class LoopbackAudioTrack extends BaseAudioTrack {
  private final LoopbackAudioSourceManager sourceManager;
  private final String deviceName;

  public LoopbackAudioTrack(LoopbackAudioSourceManager sourceManager, String deviceName) {
    super(createLoopbackTrackInfo(deviceName));

    this.sourceManager = sourceManager;
    this.deviceName = deviceName;
  }

  @Override
  public void process(LocalAudioTrackExecutor executor) throws Exception {
    AudioLoopback loopback = new AudioLoopback();
    LoopbackFrameProvider frameProvider = null;

    try {
      AudioLoopback.Format format = loopback.initialise(deviceName);
      frameProvider = new LoopbackFrameProvider(loopback, format, executor.getProcessingContext());

      executor.executeProcessingLoop(frameProvider::provideFrames, null);
    } finally {
      if (frameProvider != null) {
        frameProvider.close();
      }

      loopback.close();
    }
  }

  @Override
  public boolean isSeekable() {
    return false;
  }

  @Override
  public AudioSourceManager getSourceManager() {
    return sourceManager;
  }

  @Override
  public AudioTrack makeClone() {
    return new LoopbackAudioTrack(sourceManager, deviceName);
  }

  private static AudioTrackInfo createLoopbackTrackInfo(String deviceName) {
    return new AudioTrackInfo(
        "Output loopback" + (deviceName != null ? ": " + deviceName : ""),
        "None",
        Long.MAX_VALUE,
        "loopback: " + deviceName,
        true,
        null
    );
  }
}
