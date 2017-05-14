package com.sedmelluq.lavaplayer.loopback;

import com.sedmelluq.discord.lavaplayer.player.DefaultAudioPlayerManager;
import com.sedmelluq.discord.lavaplayer.source.AudioSourceManager;
import com.sedmelluq.discord.lavaplayer.track.AudioItem;
import com.sedmelluq.discord.lavaplayer.track.AudioReference;
import com.sedmelluq.discord.lavaplayer.track.AudioTrack;
import com.sedmelluq.discord.lavaplayer.track.AudioTrackInfo;

import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;

public class LoopbackAudioSourceManager implements AudioSourceManager {
  @Override
  public String getSourceName() {
    return "loopback";
  }

  @Override
  public AudioItem loadItem(DefaultAudioPlayerManager manager, AudioReference reference) {
    if ("loopback".equals(reference.identifier)) {
      return new LoopbackAudioTrack(this, null);
    } else if (reference.identifier.startsWith("loopback:")) {
      String deviceName = reference.identifier.substring("loopback:".length()).trim();
      return new LoopbackAudioTrack(this, deviceName);
    }

    return null;
  }

  @Override
  public boolean isTrackEncodable(AudioTrack track) {
    return false;
  }

  @Override
  public void encodeTrack(AudioTrack track, DataOutput output) throws IOException {
    throw new UnsupportedOperationException("Cannot serialize loopback track.");
  }

  @Override
  public AudioTrack decodeTrack(AudioTrackInfo trackInfo, DataInput input) throws IOException {
    return null;
  }

  @Override
  public void shutdown() {
    // Nothing to shut down
  }
}
