#!/bin/bash
# online-abuse-protection 4.3: a Neo Geo host/guest pair (headless, FRAMES frames), with ATTACK (none, flood or first)
# from attacker.py; EXTRA: more options for both sides (--forkade-online: ForkAde's match flow).
# usage: fuzz_neo.sh OUT EXE ROM FRAMES PORT ATTACK
export PATH=/c/msys64/ucrt64/bin:$PATH SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=dummy
OUT=$1; EXE=$2; ROM=$3; FRAMES=$4; PORT=$5; ATTACK=$6
HERE=$(dirname "$0")
mkdir -p "$OUT/h" "$OUT/g"
"$EXE" --rom "$ROM" --headless --host $PORT ${EXTRA:-} --frames $FRAMES --nick Host --settings "$OUT/h/s.ini" \
  --gekko-checkpoint-log "$OUT/h/state.hash" > "$OUT/host.log" 2>&1 &
hp=$!
ap=
if [ "$ATTACK" = first ]; then
  /c/msys64/ucrt64/bin/python3 "$HERE/attacker.py" $PORT $((FRAMES / 60 + 20)) 11 --first-hello > "$OUT/attacker.log" 2>&1 &
  ap=$!
  sleep 2
fi
sleep 1
"$EXE" --rom "$ROM" --headless --join 127.0.0.1:$PORT ${EXTRA:-} --frames $FRAMES --nick Guest --settings "$OUT/g/s.ini" \
  --gekko-checkpoint-log "$OUT/g/state.hash" > "$OUT/guest.log" 2>&1 &
gp=$!
if [ "$ATTACK" = flood ]; then
  sleep 1
  /c/msys64/ucrt64/bin/python3 "$HERE/attacker.py" $PORT $((FRAMES / 60 + 10)) 7 > "$OUT/attacker.log" 2>&1 &
  ap=$!
fi
( sleep $((FRAMES / 60 * 3 + 90)); kill $hp $gp 2>/dev/null ) &
wait $hp; hs=$?
wait $gp; gs=$?
[ -n "$ap" ] && kill $ap 2>/dev/null
echo "host exit $hs, guest exit $gs; host lines $(wc -l < "$OUT/h/state.hash" 2>/dev/null), guest lines $(wc -l < "$OUT/g/state.hash" 2>/dev/null)"
grep -aih "desync\|other game\|mismatch\|left\|full\|playing" "$OUT/host.log" "$OUT/guest.log" | sort | uniq -c | head -6
cat "$OUT/attacker.log" 2>/dev/null
