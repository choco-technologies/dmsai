# saitone

A sine through an audio stream node (`dmsai`, or a codec node): `saitone [NODE [FREQ_HZ [SECONDS [loop]]]]`, defaults
`/dev/dmsai0 1000 3`. It reads the stream layout with `AUDIO_GET_STREAM`, starts the stream, writes the tone (a quarter of the full scale), drains,
prints the counters of `AUDIO_GET_STATUS` and stops. With `loop` it also reads the node and reports the peak of the received samples: the
transmitter's data pad has to be wired (or routed, `dout_pin` = `din_pin` on the ESP32-S3) to the receiver's. Exit status is zero when the stream ran
without transfer errors (and, with `loop`, the tone came back).
