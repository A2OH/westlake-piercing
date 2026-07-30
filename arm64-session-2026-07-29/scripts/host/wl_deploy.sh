#!/bin/bash
# wl_deploy.sh <tag> — stage the freshly-built bridge, deploy it, restart noice cleanly.
set -e
TAG=${1:-dev}
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
cp /home/dspfac/bridge-build-arm64/out/liboh_adapter_bridge.so /home/dspfac/bridge-build-arm64/bridge_$TAG.so
cp /home/dspfac/bridge-build-arm64/bridge_$TAG.so /mnt/c/Users/dspfa/Dev/wlstage/
$HDC file send "C:\\Users\\dspfa\\Dev\\wlstage\\bridge_$TAG.so" /data/local/tmp/bridge_$TAG.so | tail -1
$HDC shell "cp /data/local/tmp/bridge_$TAG.so /data/local/tmp/asx/liboh_adapter_bridge.so && chmod 755 /data/local/tmp/asx/liboh_adapter_bridge.so && md5sum /data/local/tmp/asx/liboh_adapter_bridge.so" | tr -d '\r'
