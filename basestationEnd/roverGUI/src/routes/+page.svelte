<h1>Rover UI</h1>

<script lang="ts">
  import { onMount } from 'svelte';
  import { requestCameraStream, registerVideoElement } from '$lib/camerafeed';
  import '../lib/styles.css';

  let frontVideo = $state<HTMLVideoElement>();
  let backVideo = $state<HTMLVideoElement>();
  let leftVideo = $state<HTMLVideoElement>();
  let rightVideo = $state<HTMLVideoElement>();
  let manipVideo = $state<HTMLVideoElement>();

  onMount(() => {
    registerVideoElement('FRONT', frontVideo!);
    registerVideoElement('LEFT', backVideo!);
    registerVideoElement('RIGHT', leftVideo!);
    registerVideoElement('BACK', rightVideo!);
    registerVideoElement('MANIP', manipVideo!);
  })

  let showFront = $state(false);
  let showBack = $state(false);
  let showLeft = $state(false);
  let showRight = $state(false);
  let showManip = $state(false);

</script>

<button onclick={() => {
  showFront = true; 
  requestCameraStream("FRONT", [240,240]); // this function will change the Video object declared here
  showLeft = true; 
  requestCameraStream("LEFT", [240,240]);
  showRight = true; 
  requestCameraStream("RIGHT", [240,240]);
  showBack = true; 
  requestCameraStream("BACK", [240,240]);
  showManip = true; 
  requestCameraStream("MANIP", [240,240]);

  }}>
  Connect front camera
</button>

<div class = "videoContainer">
  <video bind:this = {frontVideo} autoplay playsinline class = "videoStream" style:display = {showFront ? 'block' : 'none'}></video>
  <video bind:this = {leftVideo} autoplay playsinline class = "videoStream" style:display = {showLeft ? 'block' : 'none'}></video>
  <video bind:this = {rightVideo} autoplay playsinline class = "videoStream" style:display = {showRight ? 'block' : 'none'}></video>
  <video bind:this = {backVideo} autoplay playsinline class = "videoStream" style:display = {showBack ? 'block' : 'none'}></video>
  <video bind:this = {manipVideo} autoplay playsinline class = "videoStream" style:display = {showManip ? 'block' : 'none'}></video>
</div>

