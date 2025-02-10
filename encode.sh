DEVICE="/dev/video0"
VIRTUAL_CAM="/dev/video10"
RESOLUTION="640:480"

BITRATE="100k"
MAXRATE="100k"
BUFSIZE="200k"

OUTPUT="rtp://127.0.0.1:5005"

ENCODER="h264_qsv"  # Use "h264_vaapi" if QSV is not available

SDP_FILE="stream.sdp"

echo "Starting H.264 streaming with Intel acceleration..."

sudo modprobe v4l2loopback devices=1 video_nr=10 exclusive_caps=1

# Create the SDP file for RTP streaming
echo "Generating SDP file: $SDP_FILE"
cat <<EOF > "$SDP_FILE"
v=0
o=- 0 0 IN IP4 127.0.0.1
s=RTP Stream
c=IN IP4 127.0.0.1
t=0 0
m=video 5005 RTP/AVP 96
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; profile-level-id=42e01f; sprop-parameter-sets=Z0IAH5WoFAFuQA==,aM4G4g==
EOF


# Open a new terminal for the first ffmpeg command (physical camera -> RTP)
gnome-terminal -- bash -c "\
ffmpeg -f v4l2 -input_format mjpeg -video_size $RESOLUTION -i '$DEVICE' \
       -vf \"scale=$RESOLUTION,fps=15\" -c:v $ENCODER -b:v $BITRATE -preset fast -tune zerolatency \
       -pix_fmt yuv420p -an -f rtp '$OUTPUT'; \
echo 'RTP stream terminated.'; \
exec bash"

# -hwaccel vaapi beofre -i if gpu is available
# change libx264 to $ENCODER when gpu is available

# Open a new terminal for the second ffmpeg command (RTP -> virtual camera)
gnome-terminal -- bash -c "\
ffmpeg -protocol_whitelist "file,rtp,udp" -re -i stream.sdp -pix_fmt yuv420p -f v4l2 /dev/video10
echo 'Virtual camera stream terminated.'; \
exec bash"



       
       