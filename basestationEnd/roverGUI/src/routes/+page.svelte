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
  import { requestCameraStream, registerVideoElement } from '$lib/cameraFunctions';
  import '../lib/cameraStyles.css';
  import '../lib/controlPanelStyle.css'

  let frontVideo = $state<HTMLVideoElement>();
  let backVideo = $state<HTMLVideoElement>();
  let leftVideo = $state<HTMLVideoElement>();
  let rightVideo = $state<HTMLVideoElement>();
  let manipVideo = $state<HTMLVideoElement>();

  $effect(() => {
    if (frontVideo) registerVideoElement('FRONT', frontVideo);
  });
  
  $effect(() => {
    if (backVideo) registerVideoElement('BACK', backVideo);
  });
  
  $effect(() => {
    if (leftVideo) registerVideoElement('LEFT', leftVideo);
  });
  
  $effect(() => {
    if (rightVideo) registerVideoElement('RIGHT', rightVideo);
  });
  
  $effect(() => {
    if (manipVideo) registerVideoElement('MANIP', manipVideo);
  });

  let focusFront = $state(true);
  let focusBack = $state(true);
  let focusLeft = $state(false);
  let focusRight = $state(false);
  let focusManip = $state(false);

  onMount(() => {
    requestCameraStream("FRONT", [240,240]);
    requestCameraStream("LEFT", [240,240]);
    requestCameraStream("RIGHT", [240,240]); 
    requestCameraStream("BACK", [240,240]);
    requestCameraStream("MANIP", [240,240]);
  });


  // Control Panel State Variables
  let activeMode = $state<'arm' | 'drivetrain'>('drivetrain');
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

  // Joint Angles (Read-only dummy data)
  let jointBase = $state(45.2);
  let jointShoulder = $state(30.5);
  let jointElbow = $state(-15.8);
  let jointWrist = $state(22.1);

  // Drivetrain Tab States
  let throttle = $state(0);
  let turn = $state(0);
  let speedLevel = $state<'slow' | 'normal' | 'fast'>('normal');

  // Motor Status (dummy data)
  let motorFL = $state({ rpm: 0, current: 0.5, temp: 25 });
  let motorFR = $state({ rpm: 0, current: 0.5, temp: 26 });
  let motorBL = $state({ rpm: 0, current: 0.5, temp: 25 });
  let motorBR = $state({ rpm: 0, current: 0.5, temp: 24 });

  // Odometry (dummy data)
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



<!-- Control Panel -->
<div class="controlPanel">
  <!-- Tab Buttons -->
  <div class="tabButtons">
    <button class="tabButton" class:active={activeMode === 'arm'} onclick={() => activeMode = 'arm'}>
      Arm
    </button>
    <button class="tabButton" class:active={activeMode === 'drivetrain'} onclick={() => activeMode = 'drivetrain'}>
      Drivetrain
    </button>
  </div>

  <div class="tabContent">
    <!-- Header Section -->
    <div class="headerSection">
      <div class="connectionStatus">
        <div class="statusDot" class:disconnected={!isConnected}></div>
        <span>{isConnected ? 'Connected' : 'Disconnected'}</span>
        <span class="latency">{latency}ms</span>
      </div>
      <div class="commandId">Command ID: {commandId}</div>
    </div>

    <!-- Status Banner -->
    <div class="statusBanner">
      <div class="statusItem" class:active={!armEstopped} class:estopped={armEstopped}>
        ARM
      </div>
      <div class="statusItem" class:active={!drivetrainEstopped} class:estopped={drivetrainEstopped}>
        DRIVE
      </div>
    </div>

    <!-- E-Stop Section -->
    <div class="estopSection">
      <div class="estopButtons">
        <button class="estopButton combined" onclick={handleEstopCombined}>
          E-STOP ALL
        </button>
        <button class="estopButton arm" onclick={handleEstopArm}>
          E-STOP ARM
        </button>
        <button class="estopButton drivetrain" onclick={handleEstopDrivetrain}>
          E-STOP DRIVETRAIN
        </button>
        <button class="estopButton clear" onclick={handleClearEstop}>
          CLEAR E-STOP
        </button>
      </div>
    </div>

    <!-- ARM TAB -->
    {#if activeMode === 'arm'}
      <!-- Cartesian data -->
      <div class="controlSection">
        <div class="sectionTitle">Cartesian Position</div>
        
        <div class="dataControl">
          <div class="dataLabel">
            <span>X Position</span>
            <span class="dataValue">{armX.toFixed(2)}m</span>
          </div>
        </div>

        <div class="dataControl">
          <div class="dataLabel">
            <span>Y Position</span>
            <span class="dataValue">{armY.toFixed(2)}m</span>
          </div>
        </div>

        <div class="dataControl">
          <div class="dataLabel">
            <span>Z Position</span>
            <span class="dataValue">{armZ.toFixed(2)}m</span>
          </div>
        </div>

        <div class="dataControl">
          <div class="dataLabel">
            <span>Wrist Angle</span>
            <span class="dataValue">{wristAngle.toFixed(1)}°</span>
          </div>
        </div>
      </div>

      <!-- Presets -->
      <div class="controlSection">
        <div class="sectionTitle">Presets</div>
        <div class="presetGrid">
          <button class="presetButton" onclick={() => handleArmPreset('home')}>Home</button>
          <button class="presetButton" onclick={() => handleArmPreset('stow')}>Stow</button>
          <button class="presetButton" onclick={() => handleArmPreset('pickup')}>Pickup</button>
          <button class="presetButton" onclick={() => handleArmPreset('extended')}>Extended</button>
        </div>
      </div>

      <!-- Joint Angles -->
      <div class="controlSection">
        <div class="sectionTitle">Joint Angles</div>
        <div class="displayGrid">
          <div class="displayItem">
            <div class="displayLabel">Base</div>
            <div class="displayValue">{jointBase.toFixed(1)}°</div>
          </div>
          <div class="displayItem">
            <div class="displayLabel">Shoulder</div>
            <div class="displayValue">{jointShoulder.toFixed(1)}°</div>
          </div>
          <div class="displayItem">
            <div class="displayLabel">Elbow</div>
            <div class="displayValue">{jointElbow.toFixed(1)}°</div>
          </div>
          <div class="displayItem">
            <div class="displayLabel">Wrist</div>
            <div class="displayValue">{jointWrist.toFixed(1)}°</div>
          </div>
        </div>
      </div>

      <!-- Arm Visualization -->
      <div class="controlSection">
        <div class="sectionTitle">Arm Visualization</div>
        <canvas class="visualizationCanvas"></canvas>
      </div>
    {/if}

    <!-- DRIVETRAIN TAB -->
    {#if activeMode === 'drivetrain'}
      <!-- Drive Controls -->
      <div class="controlSection">
        <div class="sectionTitle">Drive Controls</div>
        
        <div class="dataControl">
          <div class="dataLabel">
            <span>Throttle</span>
            <span class="dataValue">{throttle.toFixed(2)}</span>
          </div>
        </div>

        <div class="dataControl">
          <div class="dataLabel">
            <span>Turn</span>
            <span class="dataValue">{turn.toFixed(2)}</span>
          </div>
        </div>
      </div>

      <!-- Speed Level -->
      <div class="controlSection">
        <div class="sectionTitle">Speed Level</div>
        <div class="speedSelector">
          <button class="speedButton" class:active={speedLevel === 'slow'} onclick={() => speedLevel = 'slow'}>
            Slow
          </button>
          <button class="speedButton" class:active={speedLevel === 'normal'} onclick={() => speedLevel = 'normal'}>
            Normal
          </button>
          <button class="speedButton" class:active={speedLevel === 'fast'} onclick={() => speedLevel = 'fast'}>
            Fast
          </button>
        </div>
      </div>

      <!-- Motor Status -->
      <div class="controlSection">
        <div class="sectionTitle">Motor Status</div>
        <div class="displayGrid">
          <div class="displayItem">
            <div class="displayLabel">FL (251)</div>
            <div class="displayValue">{motorFL.rpm} RPM</div>
            <div style="font-size: 10px; color: #94a3b8;">{motorFL.current}A / {motorFL.temp}°C</div>
          </div>
          <div class="displayItem">
            <div class="displayLabel">FR (253)</div>
            <div class="displayValue">{motorFR.rpm} RPM</div>
            <div style="font-size: 10px; color: #94a3b8;">{motorFR.current}A / {motorFR.temp}°C</div>
          </div>
          <div class="displayItem">
            <div class="displayLabel">BL (254)</div>
            <div class="displayValue">{motorBL.rpm} RPM</div>
            <div style="font-size: 10px; color: #94a3b8;">{motorBL.current}A / {motorBL.temp}°C</div>
          </div>
          <div class="displayItem">
            <div class="displayLabel">BR (252)</div>
            <div class="displayValue">{motorBR.rpm} RPM</div>
            <div style="font-size: 10px; color: #94a3b8;">{motorBR.current}A / {motorBR.temp}°C</div>
          </div>
        </div>
      </div>

      <!-- Odometry -->
      <div class="controlSection">
        <div class="sectionTitle">Odometry</div>
        <div class="displayGrid">
          <div class="displayItem">
            <div class="displayLabel">X Position</div>
            <div class="displayValue">{odomX.toFixed(2)}m</div>
          </div>
          <div class="displayItem">
            <div class="displayLabel">Y Position</div>
            <div class="displayValue">{odomY.toFixed(2)}m</div>
          </div>
          <div class="displayItem">
            <div class="displayLabel">Heading</div>
            <div class="displayValue">{heading.toFixed(1)}°</div>
          </div>
          <div class="displayItem">
            <div class="displayLabel">Speed</div>
            <div class="displayValue">{speed.toFixed(2)}m/s</div>
          </div>
        </div>
      </div>
    {/if}
  </div>
</div>


<div class = "videoContainerFocus">
  {#if focusFront}
    <!-- svelte-ignore a11y_click_events_have_key_events --><!-- svelte-ignore a11y_no_static_element_interactions -->
    <div class="videoBaseFocus videoButton" onclick={() => {
      focusFront = false;
      requestCameraStream("FRONT", [240,240]);
    }}>
      <span class="videoLabel">Front</span>
      <video bind:this={frontVideo} autoplay playsinline class="videoBaseFocus videoStream"></video>
    </div>
  {/if}
  {#if focusLeft}
    <!-- svelte-ignore a11y_click_events_have_key_events --><!-- svelte-ignore a11y_no_static_element_interactions -->
    <div class="videoBaseFocus videoButton" onclick={() => {
      focusLeft = false;
      requestCameraStream("LEFT", [240,240]);
    }}>
      <span class="videoLabel">Left</span>
      <video bind:this={leftVideo} autoplay playsinline class="videoBaseFocus videoStream"></video>
    </div>
  {/if}
  {#if focusRight}
    <!-- svelte-ignore a11y_click_events_have_key_events --><!-- svelte-ignore a11y_no_static_element_interactions -->
    <div class="videoBaseFocus videoButton" onclick={() => {
      focusRight = false;
      requestCameraStream("RIGHT", [240,240]);
    }}>
      <span class="videoLabel">Right</span>
      <video bind:this={rightVideo} autoplay playsinline class="videoBaseFocus videoStream"></video>
    </div>
  {/if}
  {#if focusBack}
    <!-- svelte-ignore a11y_click_events_have_key_events --><!-- svelte-ignore a11y_no_static_element_interactions -->
    <div class="videoBaseFocus videoButton" onclick={() => {
      focusBack = false;
      requestCameraStream("BACK", [240,240]);
    }}>
      <span class="videoLabel">Back</span>
      <video bind:this={backVideo} autoplay playsinline class="videoBaseFocus videoStream"></video>
    </div>
  {/if}
  {#if focusManip}
    <!-- svelte-ignore a11y_click_events_have_key_events --><!-- svelte-ignore a11y_no_static_element_interactions -->
    <div class="videoBaseFocus videoButton" onclick={() => {
      focusManip = false;
      requestCameraStream("MANIP", [240,240]);
    }}>
      <span class="videoLabel">Manipulator</span>
      <video bind:this={manipVideo} autoplay playsinline class="videoBaseFocus videoStream"></video>
    </div>
  {/if}
</div>

<div class = "videoContainerBottom">
  {#if !focusFront}
    <!-- svelte-ignore a11y_consider_explicit_label-->
    <button class="videoBasePeripheral videoButton" onclick={() => focusFront = true}>
      <span class="videoLabel">Front</span>
      <video bind:this={frontVideo} autoplay playsinline muted class="videoBasePeripheral videoStream"></video>
    </button>
  {/if}
  {#if !focusLeft}
    <!-- svelte-ignore a11y_consider_explicit_label-->
    <button class="videoBasePeripheral videoButton" onclick={() => focusLeft = true}>
      <span class="videoLabel">Left</span>
      <video bind:this={leftVideo} autoplay playsinline muted class="videoBasePeripheral videoStream"></video>
    </button>
  {/if}
  {#if !focusRight}
    <!-- svelte-ignore a11y_consider_explicit_label-->
    <button class="videoBasePeripheral videoButton" onclick={() => focusRight = true}>
      <span class="videoLabel">Right</span>
      <video bind:this={rightVideo} autoplay playsinline muted class="videoBasePeripheral videoStream"></video>
    </button>
  {/if}
  {#if !focusBack}
    <!-- svelte-ignore a11y_consider_explicit_label-->
    <button class="videoBasePeripheral videoButton" onclick={() => focusBack = true}>
      <span class="videoLabel">Back</span>
      <video bind:this={backVideo} autoplay playsinline muted class="videoBasePeripheral videoStream"></video>
    </button>
  {/if}
  {#if !focusManip}
    <!-- svelte-ignore a11y_consider_explicit_label-->
    <button class="videoBasePeripheral videoButton" onclick={() => focusManip = true}>
      <span class="videoLabel">Manipulator</span>
      <video bind:this={manipVideo} autoplay playsinline muted class="videoBasePeripheral videoStream"></video>
    </button>
  {/if}
</div>

