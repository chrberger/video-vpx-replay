# video-vpx-replay
Replay .rec files containing VP8 or VP9 encoded frames

```
docker run --rm -ti --init --ipc=host --net=host -v /tmp:/tmp -v $PWD:/data -w /data -e DISPLAY=$DISPLAY vpxreplay:latest --cid=111 --name=abc --verbose  Filename.rec
```
