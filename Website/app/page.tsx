'use client';

import { ArrowDown, ArrowDownRight, ArrowUpRight, BatteryMedium, CodeXml, Image as ImageIcon, MoveUpRight, Radio, ShieldCheck, Vibrate, Zap } from 'lucide-react';
import { useEffect, useRef, useState } from 'react';
import { activeFeatureIndex } from '@/lib/active-feature';

const github = 'https://github.com/ishtms/OpenMobile';
const release = `${github}/releases/tag/v0.1.0`;

const features = [
  {
    id: 'ads', name: 'Ads', Icon: Zap, path: 'Services/OpenMobileAds',
    line: 'AdMob ads in Unreal.',
    text: 'Load and show ads from named placements. Receive ad, reward and revenue events in your Unreal project.',
    coverageLabel: 'Seven AdMob formats',
    coverage: ['Rewarded', 'Rewarded interstitial', 'Interstitial', 'App-open', 'Banner', 'Anchored adaptive banner', 'Medium rectangle (MREC)'],
    details: [
      { title: 'Reward events', text: 'Receive the earned reward amount and type. Rewarded interstitials require your game to show the reward message and a skip option before the ad.' },
      { title: 'Server-side verification', text: 'Pass a user ID and custom data for AdMob server-side verification. Handle the verification callbacks on your own reward backend.' },
      { title: 'Placement controls', text: 'Load, reload and show placements, or hide reusable banners. Check readiness and configure preloading, retries, cooldowns and frequency caps.' },
      { title: 'Consent and tracking', text: 'Use Google UMP consent forms and privacy options. Check whether ads can be requested and handle iOS tracking authorization.' },
      { title: 'Revenue and ad events', text: 'Listen for impressions, clicks, dismissals and paid events. Revenue includes value, currency, precision and the reported winning ad source.' },
      { title: 'AdMob mediation', text: 'Add AppLovin, Chartboost, Liftoff Monetize, Meta or Unity Ads. AdMob handles bidding and waterfalls configured in your account.' },
    ],
    note: 'All seven formats are implemented on Android and iOS. Format support and setup vary between mediation networks.',
  },
  {
    id: 'haptics', name: 'Haptics', Icon: Vibrate, path: 'Native/OpenMobileHaptics',
    line: 'Presets and custom haptic patterns.',
    text: 'Use feedback presets or build custom pattern assets. Control timing, intensity and playback from your game.',
    coverageLabel: 'Haptic effects and integrations',
    coverage: ['Selection feedback', 'Impact feedback', 'Success, warning and error', 'Custom pattern assets', 'Named pattern libraries', 'AHAP on iOS', 'UMG, Gameplay Abilities and Sequencer'],
    details: [
      { title: 'Feedback presets', text: 'Play selection, impact, notification and game presets from Blueprints. Use the vibration node for short pulses without a custom asset.' },
      { title: 'Pattern assets', text: 'Combine transient hits, continuous events and parameter curves. Prepare and reuse patterns through named libraries. Supported iOS devices can also play packaged AHAP resources through Core Haptics.' },
      { title: 'Playback controls', text: 'Stop or cancel effects. Supported patterns also let you pause, resume, seek and adjust intensity or sharpness.' },
      { title: 'UMG events', text: 'Add feedback to button presses, selection changes, slider steps, hover and navigation focus with the UMG plugin.' },
      { title: 'Gameplay Cues and Sequencer', text: 'Trigger feedback from Gameplay Cues or a Sequencer track. Install each integration as a separate plugin.' },
      { title: 'Player settings and fallbacks', text: 'Set master intensity, toggle feedback and configure channels and overlap policies. Check capabilities and see which fallback was used.' },
    ],
    note: 'Playback controls and effect quality depend on the device, platform and pattern. Core Haptics features on iOS and Android vibration features are reported separately.',
  },
  {
    id: 'sensors', name: 'Sensors', Icon: Radio, path: 'Native/OpenMobileSensors',
    line: 'Motion and device sensors.',
    text: 'Read motion, pose and available environmental sensors through typed listeners. Set the sample rate, delivery mode and filtering.',
    coverageLabel: 'Sensor support',
    coverage: ['Accelerometer', 'Gyroscope', 'Magnetometer', 'Gravity and linear acceleration', 'Attitude and heading', 'Shake detection', 'Steps', 'Pressure and altitude', 'Proximity', 'Ambient light on Android', 'Motion activity on iOS'],
    details: [
      { title: 'Sensor discovery', text: 'Discover available sensors and check their capabilities, permissions and accuracy.' },
      { title: 'Sampling and delivery', text: 'Poll the latest value, receive sample batches or read a bounded buffer. Use rate presets or custom frequencies to balance responsiveness and power use.' },
      { title: 'Filters and shake detection', text: 'Filter vectors with low-pass, high-pass, exponential smoothing and dead-zone options. Heading supports smoothing and a dead zone. Set shake strength, impulse count and cooldown.' },
      { title: 'Coordinates and attitude', text: 'Use device-fixed or current-screen axes. Attitude data includes quaternions, optional Euler angles and rotation matrices, plus a game reference you can recenter.' },
      { title: 'Recording', text: 'Record accelerometer, gyroscope, magnetometer, gravity and linear acceleration streams, including supported uncalibrated vector variants.' },
      { title: 'Replay', text: 'Pause, resume, seek, change speed and loop recordings. Use manual stepping to advance samples yourself.' },
    ],
    note: 'Support varies by phone and operating system. Recording and replay are limited to vector motion streams. Ambient light is Android only. Built-in motion activity uses iOS Core Motion.',
  },
  {
    id: 'device', name: 'Device info', Icon: BatteryMedium, path: 'Native/OpenMobileDevice',
    line: 'Device status and controls.',
    text: 'Read device snapshots or subscribe to changes. Monitor resources, adjust the app’s display settings and read player preferences.',
    coverageLabel: 'Device features',
    coverage: ['Battery and charging', 'Thermal and power saving', 'Memory and storage', 'Network status', 'Display and safe areas', 'Locale and accessibility', 'Clipboard and flashlight'],
    details: [
      { title: 'Battery, thermal state and resources', text: 'Monitor battery, power-saving and thermal state. Read memory and app-volume storage information, with events when monitored values change.' },
      { title: 'Window and display information', text: 'Read drawable size, density or scale, orientation and safe-area insets. Available display and refresh-rate details depend on the platform.' },
      { title: 'App display controls', text: 'Request brightness, orientation and system UI changes, or keep the screen awake. The operating system decides which requests it can apply.' },
      { title: 'Network and server checks', text: 'Monitor the current network path. Run an explicit HTTPS endpoint check to find out whether your server responds.' },
      { title: 'Locale and accessibility', text: 'Read locale, appearance, text scale, reduced-motion preferences and available accessibility state. Subscribe to changes as needed.' },
      { title: 'Clipboard, flashlight and settings', text: 'Read and write clipboard text or URLs, control a supported flashlight, and open your app’s system settings. Check capabilities before using a feature.' },
    ],
    note: 'A network route is not proof of internet access. Display, thermal and accessibility details differ between Android and iOS, and some values require physical hardware.',
  },
  {
    id: 'permissions', name: 'Permissions', Icon: ShieldCheck, path: 'Foundation/OpenMobilePermissions',
    line: 'Shared permission handling in C++.',
    text: 'Check and request permissions through a shared C++ service. OpenMobile Sensors uses it to talk to the platform permission providers.',
    coverageLabel: 'Permission operations',
    coverage: ['Status checks', 'Asynchronous requests', 'Request cancellation', 'Provider registration'],
    details: [
      { title: 'Permission status', text: 'Distinguish granted, denied, restricted, not determined and permanently denied states. Get an error if the check cannot run.' },
      { title: 'Asynchronous requests', text: 'Request a permission asynchronously. Keep the handle so you can cancel your pending request if it is no longer needed.' },
      { title: 'Sensors integration', text: 'Sensors supplies providers for Android activity-recognition and iOS motion-activity permissions, and uses this service to request them.' },
      { title: 'Custom providers', text: 'Register a C++ provider for the permission your plugin owns. The service sends status checks and requests to the available provider.' },
    ],
    note: 'This is a C++ service without standalone Blueprint request nodes. Permission support depends on the providers installed with your plugins.',
  },
  {
    id: 'media', name: 'Photo picker', Icon: ImageIcon, path: 'Native/OpenMobileMedia',
    line: 'Native photo picking for Unreal.',
    text: 'Open the system picker and get the selected image back as an Unreal texture for your UI or game.',
    coverageLabel: 'Photo picker features',
    coverage: ['Native image selection', 'Texture2D result', 'Image metadata', 'Orientation correction', 'Cancel and failure events'],
    details: [
      { title: 'System picker', text: 'Choose one image through Android’s photo picker or PHPicker on iOS. Check support before opening it.' },
      { title: 'Texture import', text: 'The plugin corrects image orientation and limits the longest texture dimension to 4096 pixels. The Blueprint async node returns an Unreal Texture2D.' },
      { title: 'Image metadata', text: 'Read original dimensions, file details and available EXIF fields, including camera information and capture time. Metadata depends on what the picker provides.' },
      { title: 'Results and cancellation', text: 'Handle picked, cancelled and failed results separately. You can cancel the picker or import in progress. Unfinished work is cancelled when its world closes.' },
    ],
    note: 'Keep a reference to the transient texture while using it. The picker handles one photo at a time, with no camera capture or video selection.',
  },
];

function Mark({ className = '' }: { className?: string }) {
  return <svg className={className} viewBox="0 0 40 40" fill="none" aria-hidden="true"><path d="M5 29V11h10l5 10 5-10h10v18H25v-9l-5 9-5-9v9H5Z" fill="currentColor" /></svg>;
}

function PlatformMap() {
  return <div className="platform-map" aria-label="Blueprints and C++ connect through OpenMobile to native Android and iOS features">
    <div className="map-top"><span className="mono">FROM YOUR GAME</span><span className="map-plus" aria-hidden="true">+</span></div>
    <div className="map-inputs"><span>Blueprints</span><span>C++</span></div>
    <div className="map-connector" aria-hidden="true"><span /><span /></div>
    <div className="map-core"><Mark /><span>OpenMobile</span><span className="core-status" aria-hidden="true" /></div>
    <div className="map-trunk" aria-hidden="true" />
    <div className="map-features" aria-hidden="true">{features.map(({ id, Icon }) => <span key={id}><Icon strokeWidth={1.5} /></span>)}</div>
    <div className="map-connector bottom" aria-hidden="true"><span /><span /></div>
    <div className="map-inputs map-platforms"><span>Android</span><span>iOS</span></div>
    <div className="map-bottom"><span className="mono">TO THE DEVICE</span><ArrowDownRight size={24} strokeWidth={1.5} aria-hidden="true" /></div>
  </div>;
}

export default function Home() {
  const [activeFeature, setActiveFeature] = useState(features[0].id);
  const sectionsRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const sections = Array.from(sectionsRef.current?.querySelectorAll<HTMLElement>('[data-feature]') ?? []);
    if (!sections.length) return;

    let frame = 0;
    const updateActive = () => {
      frame = 0;
      const tops = sections.map(section => section.getBoundingClientRect().top);
      const index = activeFeatureIndex(tops, Math.min(180, window.innerHeight / 2));
      setActiveFeature(features[index].id);
    };
    const scheduleUpdate = () => {
      if (!frame) frame = window.requestAnimationFrame(updateActive);
    };
    const observer = new ResizeObserver(scheduleUpdate);
    sections.forEach(section => observer.observe(section));
    window.addEventListener('scroll', scheduleUpdate, { passive: true });
    window.addEventListener('resize', scheduleUpdate);
    scheduleUpdate();

    return () => {
      observer.disconnect();
      window.removeEventListener('scroll', scheduleUpdate);
      window.removeEventListener('resize', scheduleUpdate);
      window.cancelAnimationFrame(frame);
    };
  }, []);

  return <>
    <a href="#main" className="skip-link">Skip to content</a>
    <header className="site-header wrap">
      <a className="wordmark" href="#" aria-label="OpenMobile home"><Mark />OpenMobile</a>
      <nav aria-label="Main navigation"><a href="#plugins">The plugins</a><a href="#why">The reason</a><a href="#next">What’s next</a></nav>
      <a href={github} className="header-github"><CodeXml size={18} strokeWidth={1.8} aria-hidden="true" /><span>GitHub</span><ArrowUpRight size={16} aria-hidden="true" /></a>
    </header>
    <main id="main">
      <section className="hero wrap" aria-labelledby="hero-title">
        <div className="hero-grid">
          <div className="hero-copy"><h1 id="hero-title">UNREAL.<br />MORE<br /><span>MOBILE.</span><span className="title-spark" aria-hidden="true">✳</span></h1></div>
          <div className="hero-side"><p className="hero-description">Mobile plugins for<br />your Unreal project.</p><PlatformMap /><p className="map-caption">Native Android and iOS features,<br className="desktop-break" /> with Blueprint and C++ APIs.</p></div>
        </div>
        <div className="hero-bottom"><a className="button button-dark" href={release}>Get the plugins <ArrowUpRight size={22} aria-hidden="true" /></a><span className="hero-release mono">v0.1.0 SOURCE RELEASE</span><a href="#plugins" className="explore-link">Explore the collection <ArrowDown size={18} aria-hidden="true" /></a></div>
      </section>
      <div className="spec-strip"><div className="wrap spec-inner"><span>16 separate plugins</span><span>Blueprint + C++</span><span>Android + iOS</span><span>Pick what you need <ArrowDownRight size={18} aria-hidden="true" /></span></div></div>
      <section className="plugins-section wrap" id="plugins" aria-labelledby="plugins-title">
        <div className="section-heading"><span className="section-index mono">THE COLLECTION</span><div><h2 id="plugins-title">The plugins<br /><span>and what they do.</span></h2><p>See which formats, controls and device features each plugin supports. Install the ones your project needs.</p></div></div>
        <div className="feature-collection">
          <nav className="feature-list" aria-label="Explore native features">{features.map(({ id, name }) => <a href={`#plugin-${id}`} key={id} className="feature-trigger" aria-current={activeFeature === id ? 'location' : undefined}><span className="feature-tab-dot" aria-hidden="true" /><span>{name}</span><ArrowUpRight className="feature-arrow" size={19} aria-hidden="true" /></a>)}</nav>
          <div className="feature-sections" ref={sectionsRef}>
          {features.map(({ id, name, line, text, coverageLabel, coverage, details, note, Icon, path }) => (
            <section id={`plugin-${id}`} key={id} className="feature-panel" data-feature={id} aria-labelledby={`plugin-${id}-title`}>
              <div className="panel-top"><span className="mono">OPENMOBILE {name.toUpperCase()}</span><a href={`${github}/tree/main/${path}`} aria-label={`View ${name} source on GitHub`}>View source <ArrowUpRight size={17} aria-hidden="true" /></a></div>
              <div className="panel-body">
                <div className="panel-copy"><h3 id={`plugin-${id}-title`}>{line}</h3><p>{text}</p></div>
                <div className={`feature-symbol symbol-${id}`} aria-hidden="true"><Icon strokeWidth={0.75} /></div>
              </div>
              <div className="feature-coverage">
                <h4>{coverageLabel}</h4>
                <ul>{coverage.map(item => <li key={item}>{item}</li>)}</ul>
              </div>
              <div className="feature-details">
                {details.map(detail => <article key={detail.title}><h4>{detail.title}</h4><p>{detail.text}</p></article>)}
              </div>
              <div className="panel-bottom"><p>{note}</p></div>
            </section>
          ))}
          </div>
        </div>
        <p className="collection-note"><span className="note-star" aria-hidden="true">✳</span> Every feature needs Core. Haptics integrations and AdMob mediation adapters are separate downloads.</p>
      </section>
      <section className="why-section" id="why" aria-labelledby="why-title"><div className="wrap why-grid"><div className="why-label"><span className="section-index mono">A NOTE FROM ISHTMEET</span><div className="why-monogram" aria-hidden="true"><Mark /></div><span className="mono why-signoff">BUILT FOR MY OWN PROJECTS.</span></div><div className="why-copy"><h2 id="why-title">I want Unreal to be<br /><span>better on mobile.</span></h2><p>Mobile features are hard to get right in Unreal. I’ve spent years building these plugins and using them in my own projects.</p><p>I’m open sourcing them so other developers can use this work and help improve it.</p><p className="signature">Ishtmeet Singh <span>Creator of OpenMobile</span></p></div></div></section>
      <section className="next-section wrap" id="next" aria-labelledby="next-title"><div className="section-heading"><span className="section-index mono">THE ROAD AHEAD</span><div><h2 id="next-title">What I’m<br /><span>working on next.</span></h2><p>The plugins are available now. I’m working on the guides and more device testing.</p></div></div><div className="roadmap"><article><span className="roadmap-status mono"><i className="status-dot" />AVAILABLE NOW</span><h3>The first release</h3><p>All 16 plugins are available as separate source downloads for Unreal Engine 5.8.</p><a href={release}>Download v0.1.0 <ArrowUpRight size={18} aria-hidden="true" /></a></article><article><span className="roadmap-status mono">COMING NEXT</span><h3>Docs and tutorials</h3><p>I’m writing guides and examples for each plugin. They’ll take some time.</p><span className="roadmap-footnote">Getting started is in the README for now.</span></article><article><span className="roadmap-status mono">ONGOING</span><h3>More device testing</h3><p>I want to test on more phones and fix the issues people find in their projects.</p><a href={`${github}/issues`}>Share what you find <ArrowUpRight size={18} aria-hidden="true" /></a></article></div></section>
      <section className="download-section" aria-labelledby="download-title"><div className="wrap"><div className="download-top"><span className="mono">YOUR NEXT MOBILE PROJECT</span><span className="mono">START WITH v0.1.0</span></div><div className="download-grid"><h2 id="download-title">TRY OPENMOBILE<br />IN YOUR PROJECT<span className="end-dot">.</span></h2><div className="download-copy"><p>Download Core, then the features your project needs. These are source plugins, so you’ll need Unreal Engine 5.8 and a C++ build toolchain.</p><a className="button button-dark" href={release}>Pick your plugins <ArrowUpRight size={22} aria-hidden="true" /></a><a href={`${github}#getting-started`} className="setup-link">Read the setup steps <MoveUpRight size={16} aria-hidden="true" /></a></div></div></div></section>
    </main>
    <footer className="site-footer wrap"><a className="wordmark" href="#"><Mark />OpenMobile</a><p>An open source project by Ishtmeet.</p><div><a href={`${github}/issues`}>Issues <ArrowUpRight size={15} aria-hidden="true" /></a><a href={github}>GitHub <ArrowUpRight size={15} aria-hidden="true" /></a><a href="#main" aria-label="Back to top">Back up <ArrowUpRight size={15} aria-hidden="true" /></a></div></footer>
  </>;
}
