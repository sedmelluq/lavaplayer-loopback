# LavaPlayer Loopback - Audio output loopback as an audio track for LavaPlayer

This project implements an audio source manager for LavaPlayer which provides a track mirroring the audio going to the primary audio device on Windows.

#### Maven package

* Repository: jcenter
* Artifact: **com.sedmelluq:lavaplayer-loopback:1.0.3**

Using in Gradle:
```groovy
repositories {
  jcenter()
}

dependencies {
  compile 'com.sedmelluq:lavaplayer-loopback:1.0.3'
}
```

Using in Maven:
```xml
<repositories>
  <repository>
    <id>central</id>
    <name>bintray</name>
    <url>http://jcenter.bintray.com</url>
  </repository>
</repositories>

<dependencies>
  <dependency>
    <groupId>com.sedmelluq</groupId>
    <artifactId>lavaplayer-loopback</artifactId>
    <version>1.0.3</version>
  </dependency>
</dependencies>
```


## Usage

To register the source:
```java
playerManager.registerSourceManager(new LoopbackAudioSourceManager());
```

Then it can be loaded with:
```java
playerManager.loadItem("loopback", ...);
```

An alternative way is to simply create the track directly:
```java
LoopbackAudioSourceManager loopbackSource = new LoopbackAudioSourceManager();
LoopbackAudioTrack loopbackTrack = new LoopbackAudioTrack(loopbackSource);
```
