<style>
  :global(body) {
    background-color: #0a0a2e;
    margin: 0;
    padding: 0;
  }

  :global(body::before) {
    content: "";
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background-image: url('../lib/assets/the-constellation-cygnus.jpg');
    background-size: cover;
    background-position: center;
    opacity: 0.3;
    z-index: -1;
  }
</style>

<script lang="ts">
  import { onMount } from 'svelte';
  import { registerVideoElement, frameWatchdog, connectSocket, takePhoto} from '$lib/cameraFunctions';
  import { cameras } from '$lib/webRtcStreamObject';
  import '../lib/cameraStyles.css';
  import '../lib/controlPanelStyle.css'

  let frontVideo = $state<HTMLVideoElement>();
  let backVideo = $state<HTMLVideoElement>();
  let leftVideo = $state<HTMLVideoElement>();
  let rightVideo = $state<HTMLVideoElement>();
  let manipVideo = $state<HTMLVideoElement>();

  $effect(() => {
    if (frontVideo) registerVideoElement(cameras.FRONT, frontVideo);
  });
  
  $effect(() => {
    if (backVideo) registerVideoElement(cameras.BACK, backVideo);
  });
  
  $effect(() => {
    if (leftVideo) registerVideoElement(cameras.LEFT, leftVideo);
  });
  
  $effect(() => {
    if (rightVideo) registerVideoElement(cameras.RIGHT, rightVideo);
  });
  
  $effect(() => {
    if (manipVideo) registerVideoElement(cameras.MANIP, manipVideo);
  });

  onMount(() => {
    connectSocket();
    frameWatchdog(cameras.FRONT);
    frameWatchdog(cameras.LEFT);
    frameWatchdog(cameras.RIGHT); 
    //frameWatchdog(cameras.BACK);
    //frameWatchdog(cameras.MANIP);
  });

  // ================= Control panel stuff ====================

  // Control Panel State Variables
  let activeMode = $state<'arm' | 'drivetrain' | 'science'>('drivetrain');
  let isConnected = $state(true);
  let latency = $state(45);
  let commandId = $state(0);

  // E-Stop States
  let armEstopped = $state(false);
  let drivetrainEstopped = $state(false);

  // Arm Tab States
  let armX = $state(0.5);
  let armY = $state(0.5);
  let armZ = $state(0.3);
  let wristAngle = $state(0);

  // Joint Angles 
  let jointBase = $state(45.2);
  let jointShoulder = $state(30.5);
  let jointElbow = $state(-15.8);
  let jointWrist = $state(22.1);

  // Drivetrain Tab States
  let throttle = $state(0);
  let turn = $state(0);
  let speedLevel = $state<'slow' | 'normal' | 'fast'>('normal');

  // Motor Status 
  let motorFL = $state({ rpm: 0, current: 0.5, temp: 25 });
  let motorFR = $state({ rpm: 0, current: 0.5, temp: 26 });
  let motorBL = $state({ rpm: 0, current: 0.5, temp: 25 });
  let motorBR = $state({ rpm: 0, current: 0.5, temp: 24 });

  // Odometry 
  let odomX = $state(0.0);
  let odomY = $state(0.0);
  let heading = $state(0.0);
  let speed = $state(0.0);
  let batteryVoltage = $state(12.4);

  // Joystick & Input
  let deadzoneValue = $state(0.15);
  let deadzoneEnabled = $state(true);

  // Functions
  function handleEstopCombined() {
    armEstopped = true;
    drivetrainEstopped = true;
    console.log('Combined E-Stop activated');
  }

  function handleEstopArm() {
    armEstopped = true;
    console.log('Arm E-Stop activated');
  }

  function handleEstopDrivetrain() {
    drivetrainEstopped = true;
    console.log('Drivetrain E-Stop activated');
  }

  function handleClearEstop() {
    armEstopped = false;
    drivetrainEstopped = false;
    console.log('E-Stops cleared');
  }

  function handleArmPreset(preset: string) {
    console.log(`Arm preset: ${preset}`);
    // Add your preset logic here
  }

  function incrementCommandId() {
    commandId++;
  }

</script>

<!-- Camera Panel ======================= -->
<div class="allVideosContainer">

  <!-- svelte-ignore a11y_click_events_have_key_events -->
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="frontAndWings">
    <div class="videoWrapper wings">
      <span class="videoLabel">Left</span>
      <video bind:this={leftVideo} autoplay playsinline muted class="videoStream" style="transform: rotate(180deg)"></video>
      <button type="button" class="cameraIconBtn" on:click={() => takePhoto(leftVideo)}>
        <img src="./src/lib/assets/cameraIcon.jpg" alt="Take photo" class="cameraIcon" />
      </button>
    </div>

    <div class="videoWrapper front">
      <span class="videoLabel">Front</span>
      <video bind:this={frontVideo} autoplay playsinline muted class="videoStream"></video>
      <button type="button" class="cameraIconBtn" on:click={() => takePhoto(frontVideo)}>
        <img src="./src/lib/assets/cameraIcon.jpg" alt="Take photo" class="cameraIcon" />
      </button>
    </div>

    <div class="videoWrapper wings">
      <span class="videoLabel">Right</span>
      <video bind:this={rightVideo} autoplay playsinline muted class="videoStream" style="transform: rotate(180deg)"></video>
      <button type="button" class="cameraIconBtn" on:click={() => takePhoto(rightVideo)}>
        <img src="./src/lib/assets/cameraIcon.jpg" alt="Take photo" class="cameraIcon" />
      </button>
    </div>
  </div>
</div>

