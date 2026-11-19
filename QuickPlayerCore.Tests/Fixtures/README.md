These fixtures contain a generated 440 Hz sine wave (one second, stereo, 44.1 kHz).
They contain no recorded or copyrighted music. Generated with FFmpeg using:

```
ffmpeg -f lavfi -i sine=frequency=440:sample_rate=44100:duration=1 -ac 2 -c:a <codec> \
  -metadata title="Песня Native AOT" -metadata artist="Исполнитель" -metadata album="Альбом" tone.<ext>
```

Codecs: libmp3lame (mp3), aac (m4a), flac (flac), pcm_s16le (wav).
`tone-vbr.mp3` additionally uses `-q:a 4` to cover VBR duration and title rewriting.
`tone-multiple-artists.m4a` copies the AAC fixture with performers `Первый`, `Второй`
written by the previous TagLibSharp 2.3.0 implementation. It guards first-performer
compatibility for Apple tag values containing a semicolon-separated list.
The tests check known metadata and preservation after writing the title. A separate migration
smoke check uses FFprobe and decoded PCM hashes to verify the container outside TagLib.
