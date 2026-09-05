import type { Metadata } from 'next';
import { Barlow_Condensed, DM_Sans, IBM_Plex_Mono } from 'next/font/google';
import './globals.css';

const display = Barlow_Condensed({ variable: '--font-display', subsets: ['latin'], weight: ['600', '700'] });
const sans = DM_Sans({ variable: '--font-body', subsets: ['latin'] });
const mono = IBM_Plex_Mono({ variable: '--font-label', subsets: ['latin'], weight: ['400', '500'] });

export const metadata: Metadata = {
  title: 'OpenMobile - Unreal, more mobile',
  description: 'Open source Android and iOS plugins for Unreal Engine. Ads, haptics, sensors, device info, permissions, and photo picking through Blueprints and C++.',
  icons: { icon: '/icon.svg' },
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return <html lang="en"><body className={`${display.variable} ${sans.variable} ${mono.variable}`}>{children}</body></html>;
}
