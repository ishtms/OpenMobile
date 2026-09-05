'use client';

import { ArrowDown, ArrowDownRight, ArrowUpRight, BatteryMedium, CodeXml, Image as ImageIcon, MoveUpRight, Radio, ShieldCheck, Vibrate, Zap } from 'lucide-react';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';

const github = 'https://github.com/ishtms/OpenMobile';
const release = `${github}/releases/tag/v0.1.0`;

const features = [
  {
    id: 'ads', name: 'Ads', Icon: Zap, path: 'Services/OpenMobileAds',
    line: 'From rewarded ads to app-open.',
    text: 'Run AdMob through named placements in Unreal. Choose a format, preload it, and react to ad, reward and revenue events from your game.',
    coverageLabel: 'Seven AdMob formats',
    coverage: ['Rewarded', 'Rewarded interstitial', 'Interstitial', 'App-open', 'Banner', 'Anchored adaptive banner', 'Medium rectangle (MREC)'],
    details: [
      { title: 'Rewards your game can act on', text: 'Receive the earned reward amount and reward type. For rewarded interstitials, your game presents the reward message and a skip option before showing the ad.' },
      { title: 'Server verification fields', text: 'Attach a user ID and custom data to rewarded ads for AdMob server-side verification. Connect those callbacks to your own reward backend.' },
      { title: 'Placement controls', text: 'Load, reload and show placements. Hide reusable banners, check readiness, and configure automatic preloading, retries, cooldowns and frequency caps.' },
      { title: 'Consent and tracking', text: 'Use Google UMP consent forms and privacy options. Check request eligibility and handle iOS tracking authorization from the same ads flow.' },
      { title: 'Revenue and ad events', text: 'Listen for impressions, clicks, dismissals and paid events. Revenue includes value, currency, precision and the reported winning ad source.' },
      { title: 'Mediation when you need it', text: 'Add AppLovin, Chartboost, Liftoff Monetize, Meta or Unity Ads. AdMob handles bidding and waterfalls configured in your account.' },
    ],
    note: 'These seven AdMob formats are implemented on Android and iOS. Each mediation network has its own format support and setup requirements.',
  },
  {
    id: 'haptics', name: 'Haptics', Icon: Vibrate, path: 'Native/OpenMobileHaptics',
    line: 'A button click. A hit. A whole pattern.',
    text: 'Start with familiar feedback presets or author your own haptic assets. Keep the timing, intensity and playback controls close to your gameplay.',
    coverageLabel: 'Ready for gameplay and UI',
    coverage: ['Selection feedback', 'Impact feedback', 'Success, warning and error', 'Custom pattern assets', 'Named pattern libraries', 'AHAP on iOS', 'UMG, Gameplay Abilities and Sequencer'],
    details: [
      { title: 'Quick feedback nodes', text: 'Play selection, impact, notification and game presets from Blueprints. A simple vibration node covers short pulses without a custom asset.' },
      { title: 'Reusable pattern assets', text: 'Build patterns from transient hits, continuous events and parameter curves. Use named libraries to prepare reusable feedback. On supported iOS devices, play packaged AHAP resources through Core Haptics.' },
      { title: 'Control the playback', text: 'Stop or cancel a playing effect. Supported patterns also expose pause, resume, seeking and changes to intensity and sharpness.' },
      { title: 'UMG that feels responsive', text: 'Bind feedback to button presses, selection changes, slider steps, hover and navigation focus through the optional widget integration.' },
      { title: 'Abilities and cinematics', text: 'Trigger feedback from Gameplay Cues or put haptics on a Sequencer track. Each integration is a separate plugin.' },
      { title: 'Player settings and device limits', text: 'Set master intensity, enable or disable feedback, and use channels and overlap policies. Capability checks and fallback results tell you what was used.' },
    ],
    note: 'Playback controls and effect quality depend on the device, platform and pattern. Core Haptics features on iOS and Android vibration features are reported separately.',
  },
  {
    id: 'sensors', name: 'Sensors', Icon: Radio, path: 'Native/OpenMobileSensors',
    line: 'More than a raw accelerometer.',
    text: 'Read motion, pose and available environmental sensors through typed listeners. Choose how often you sample, how you receive the data and how much filtering you need.',
    coverageLabel: 'Discover what the device supports',
    coverage: ['Accelerometer', 'Gyroscope', 'Magnetometer', 'Gravity and linear acceleration', 'Attitude and heading', 'Shake detection', 'Steps', 'Pressure and altitude', 'Proximity', 'Ambient light on Android', 'Motion activity on iOS'],
    details: [
      { title: 'Find the right sensor', text: 'Discover available sensors and inspect their capabilities, permissions and accuracy. Unsupported hardware is reported before you build gameplay around it.' },
      { title: 'Choose the delivery', text: 'Poll the latest value, receive batches of samples or read a bounded buffer. Rate presets and custom frequencies let you tune responsiveness and power use.' },
      { title: 'Clean up the motion', text: 'Apply low-pass or high-pass filters, exponential smoothing and dead zones to vector streams. Heading supports smoothing and a dead zone. Detect shakes with configurable strength, impulse count and cooldown.' },
      { title: 'Work in useful coordinates', text: 'Use device-fixed or current-screen axes. Attitude streams offer quaternions, optional Euler angles and rotation matrices, with a recenterable game reference.' },
      { title: 'Record a real movement', text: 'Save supported vector motion streams, including accelerometer, gyroscope, magnetometer, gravity and linear acceleration. Uncalibrated vector variants are supported too.' },
      { title: 'Replay while you iterate', text: 'Pause, resume, seek, change speed and loop a recording. Manual replay stepping gives you control over how recorded samples advance.' },
    ],
    note: 'Availability varies by phone and operating system. Recording and replay support vector motion streams, not every sensor family. Ambient light is Android only, and built-in motion activity uses iOS Core Motion.',
  },
  {
    id: 'device', name: 'Device info', Icon: BatteryMedium, path: 'Native/OpenMobileDevice',
    line: 'Know the phone. Adapt the game.',
    text: 'Read a snapshot when you need it or monitor changes while your game runs. Device status, display controls and player preferences are available in one place.',
    coverageLabel: 'Useful information from the device',
    coverage: ['Battery and charging', 'Thermal and power saving', 'Memory and storage', 'Network status', 'Display and safe areas', 'Locale and accessibility', 'Clipboard and flashlight'],
    details: [
      { title: 'Battery, heat and resources', text: 'Watch battery and power-saving state, react to thermal changes, and read memory and app-volume storage information. Monitoring provides change events.' },
      { title: 'Screen and window information', text: 'Read drawable size, density or scale, orientation and safe-area insets. Display and refresh-rate information follows what the platform can report.' },
      { title: 'Controls for the current app', text: 'Request a brightness override, keep the screen awake, and set orientation or system UI preferences. The operating system still decides what it can apply.' },
      { title: 'Network status and endpoint checks', text: 'Monitor the current network path. When you need to know whether your server answers, run an explicit HTTPS endpoint check.' },
      { title: 'Respect player preferences', text: 'Read locale, appearance, preferred text scale, reduced-motion settings and available accessibility state. Subscribe to changes instead of polling everything.' },
      { title: 'Everyday native tools', text: 'Read or write clipboard text and URLs, control an available flashlight, and open your app’s system settings. Capability checks report what the device supports.' },
    ],
    note: 'A network route is not proof of internet access. Display, thermal and accessibility details differ between Android and iOS, and some values require physical hardware.',
  },
  {
    id: 'permissions', name: 'Permissions', Icon: ShieldCheck, path: 'Foundation/OpenMobilePermissions',
    line: 'One place to handle permission results.',
    text: 'A shared C++ service for checking and requesting permissions owned by OpenMobile providers. It is the foundation used by the Sensors permission flow.',
    coverageLabel: 'The shared permission service',
    coverage: ['Status checks', 'Asynchronous requests', 'Request cancellation', 'Provider registration'],
    details: [
      { title: 'Know the current answer', text: 'Read a typed result that distinguishes granted, denied, restricted, not determined and permanently denied states, along with an error when a check cannot run.' },
      { title: 'Own the request', text: 'Request a permission asynchronously and keep its handle. Cancel your pending request when the owning feature no longer needs the result.' },
      { title: 'Connected to Sensors', text: 'The Sensors backends supply Android activity-recognition and iOS motion-activity permission providers. The sensor APIs use this shared service.' },
      { title: 'Add a provider in C++', text: 'Register a provider for the permission your plugin owns. The service routes status checks and requests to the available provider.' },
    ],
    note: 'This plugin exposes a C++ service. It does not provide standalone Blueprint permission-request nodes or a built-in provider for every system permission.',
  },
  {
    id: 'media', name: 'Photo picker', Icon: ImageIcon, path: 'Native/OpenMobileMedia',
    line: 'Pick a photo. Get an Unreal texture.',
    text: 'Let a player choose an image through the native system picker, then use the returned texture in your UI or game. The image import and result handling are part of the plugin.',
    coverageLabel: 'From the photo library to your game',
    coverage: ['Native image selection', 'Texture2D result', 'Image metadata', 'Orientation correction', 'Cancel and failure events'],
    details: [
      { title: 'A familiar system picker', text: 'Choose one image using Android’s photo picker flow or PHPicker on iOS. Check whether photo picking is supported before opening it.' },
      { title: 'A display-ready texture', text: 'The import corrects image orientation and limits the longest texture dimension to 4096 pixels. The Blueprint async node returns an Unreal Texture2D.' },
      { title: 'Useful image details', text: 'Read the original dimensions, file details and available EXIF metadata, such as camera information and capture time. Fields depend on what the picker provides.' },
      { title: 'Clear outcomes and cleanup', text: 'Handle picked, cancelled and failed results separately. Cancel the picker or an image import in progress, and unfinished work is cancelled when its world closes.' },
    ],
    note: 'Keep a reference to the returned transient texture while you use it. This plugin picks a single photo. It is not a camera capture or video picker.',
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
          <div className="hero-side"><p className="hero-description">The native side of your game.<br />A little easier to get right.</p><PlatformMap /><p className="map-caption">Android and iOS features, ready for<br className="desktop-break" /> the way you work in Unreal.</p></div>
        </div>
        <div className="hero-bottom"><a className="button button-dark" href={release}>Get the plugins <ArrowUpRight size={22} aria-hidden="true" /></a><span className="hero-release mono">v0.1.0 SOURCE RELEASE</span><a href="#plugins" className="explore-link">Explore the collection <ArrowDown size={18} aria-hidden="true" /></a></div>
      </section>
      <div className="spec-strip"><div className="wrap spec-inner"><span>16 separate plugins</span><span>Blueprint + C++</span><span>Android + iOS</span><span>Pick what you need <ArrowDownRight size={18} aria-hidden="true" /></span></div></div>
      <section className="plugins-section wrap" id="plugins" aria-labelledby="plugins-title">
        <div className="section-heading"><span className="section-index mono">THE COLLECTION</span><div><h2 id="plugins-title">The phone can do a lot.<br /><span>So should your game.</span></h2><p>Explore the formats, controls and device features in each plugin. Start with what your game needs, then add the rest when you need it.</p></div></div>
        <Tabs defaultValue="ads" className="feature-tabs">
          <TabsList className="feature-list" aria-label="Explore native features">{features.map(({ id, name, Icon }) => <TabsTrigger value={id} key={id} className="feature-trigger"><Icon className="feature-tab-icon" size={21} strokeWidth={1.5} aria-hidden="true" /><span>{name}</span><ArrowUpRight className="feature-arrow" size={19} aria-hidden="true" /></TabsTrigger>)}</TabsList>
          {features.map(({ id, name, line, text, coverageLabel, coverage, details, note, Icon, path }) => (
            <TabsContent value={id} key={id} className="feature-panel">
              <div className="panel-top"><span className="mono">OPENMOBILE {name.toUpperCase()}</span></div>
              <div className="panel-body">
                <div className={`feature-symbol symbol-${id}`} aria-hidden="true"><Icon strokeWidth={0.75} /></div>
                <div className="panel-copy"><h3>{line}</h3><p>{text}</p></div>
              </div>
              <div className="feature-coverage">
                <h4>{coverageLabel}</h4>
                <ul>{coverage.map(item => <li key={item}>{item}</li>)}</ul>
              </div>
              <div className="feature-details">
                {details.map(detail => <article key={detail.title}><h4>{detail.title}</h4><p>{detail.text}</p></article>)}
              </div>
              <div className="panel-bottom"><p>{note}</p><a href={`${github}/tree/main/${path}`} aria-label={`View ${name} source on GitHub`}>View source <ArrowUpRight size={17} aria-hidden="true" /></a></div>
            </TabsContent>
          ))}
        </Tabs>
        <p className="collection-note"><span className="note-star" aria-hidden="true">✳</span> Core connects the collection. Optional integrations cover haptics in Unreal and mediation for AdMob.</p>
      </section>
      <section className="why-section" id="why" aria-labelledby="why-title"><div className="wrap why-grid"><div className="why-label"><span className="section-index mono">A NOTE FROM ISHTMEET</span><div className="why-monogram" aria-hidden="true"><Mark /></div><span className="mono why-signoff">A DEVELOPER, LIKE YOU.</span></div><div className="why-copy"><h2 id="why-title">I want Unreal to be<br /><span>better on mobile.</span></h2><p>Mobile features are hard to get right in Unreal. I’ve spent years working through the same problems, building these plugins and using them in my own projects.</p><p>I’m open sourcing that work because I’d like it to be useful to you too. If it saves you a few late nights, or helps you get your game onto a phone, that’s a good start.</p><p className="signature">Ishtmeet Singh <span>Creator of OpenMobile</span></p></div></div></section>
      <section className="next-section wrap" id="next" aria-labelledby="next-title"><div className="section-heading"><span className="section-index mono">THE ROAD AHEAD</span><div><h2 id="next-title">Out in the open.<br /><span>Still moving forward.</span></h2><p>The first release is a starting point. Here’s where I want to spend time next.</p></div></div><div className="roadmap"><article><span className="roadmap-status mono"><i className="status-dot" />AVAILABLE NOW</span><h3>The first release</h3><p>All 16 plugins are available as separate source downloads for Unreal Engine 5.8.</p><a href={release}>Download v0.1.0 <ArrowUpRight size={18} aria-hidden="true" /></a></article><article><span className="roadmap-status mono">COMING NEXT</span><h3>Docs and tutorials</h3><p>I’m writing the guides and examples for each plugin. They’ll take a little time to get right.</p><span className="roadmap-footnote">Getting started is in the README for now.</span></article><article><span className="roadmap-status mono">ONGOING</span><h3>More time on devices</h3><p>Testing on more phones, fixing the things we find, and learning from the games you build with it.</p><a href={`${github}/issues`}>Share what you find <ArrowUpRight size={18} aria-hidden="true" /></a></article></div></section>
      <section className="download-section" aria-labelledby="download-title"><div className="wrap"><div className="download-top"><span className="mono">YOUR NEXT MOBILE PROJECT</span><span className="mono">START WITH v0.1.0</span></div><div className="download-grid"><h2 id="download-title">LET’S MAKE<br />MOBILE BETTER<span className="end-dot">.</span></h2><div className="download-copy"><p>Download Core, then the features your project needs. These are source plugins, so you’ll need Unreal Engine 5.8 and a C++ build toolchain.</p><a className="button button-dark" href={release}>Pick your plugins <ArrowUpRight size={22} aria-hidden="true" /></a><a href={`${github}#getting-started`} className="setup-link">Read the setup steps <MoveUpRight size={16} aria-hidden="true" /></a></div></div></div></section>
    </main>
    <footer className="site-footer wrap"><a className="wordmark" href="#"><Mark />OpenMobile</a><p>Built by Ishtmeet. Open to everyone.</p><div><a href={`${github}/issues`}>Issues <ArrowUpRight size={15} aria-hidden="true" /></a><a href={github}>GitHub <ArrowUpRight size={15} aria-hidden="true" /></a><a href="#main" aria-label="Back to top">Back up <ArrowUpRight size={15} aria-hidden="true" /></a></div></footer>
  </>;
}
