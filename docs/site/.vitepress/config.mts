import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'ADT (Qt)',
  description: 'Guides and reference for displaying EPICS process-variable arrays with ADT (Qt).',
  lang: 'en-US',
  base: process.env.DOCS_BASE || '/',
  outDir: './dist',
  srcExclude: ['public/**', '_includes/**'],
  cleanUrls: false,
  head: [['meta', { name: 'theme-color', content: '#126384' }]],
  themeConfig: {
    siteTitle: 'ADT (Qt) / docs',
    search: { provider: 'local' },
    outline: { level: [2, 3], label: 'On this page' },
    nav: [
      { text: 'Guide', link: '/get-started/first-display' },
      { text: 'Reference', link: '/reference/pv-files' },
      { text: 'Downloads', link: '/reference/manuals' },
      { text: 'Repository', link: 'https://github.com/rtsoliday/adt' }
    ],
    sidebar: [
      { text: 'GET STARTED', items: [
        { text: 'Overview', link: '/' },
        { text: 'Install & build', link: '/get-started/install' },
        { text: 'Open your first display', link: '/get-started/first-display' }
      ]},
      { text: 'OPERATE', collapsed: false, items: [
        { text: 'Display areas', link: '/operate/display-areas' },
        { text: 'Logarithmic scales', link: '/operate/log-scale' },
        { text: 'Zoom & pan', link: '/operate/zoom-area' },
        { text: 'Status checking', link: '/operate/status' },
        { text: 'Stored sets & differences', link: '/operate/stored-sets' },
        { text: 'Snapshots & plots', link: '/operate/snapshots-plots' }
      ]},
      { text: 'REFERENCE', collapsed: true, items: [
        { text: 'PV files', link: '/reference/pv-files' },
        { text: 'Configuration file', link: '/reference/configuration' },
        { text: 'Command-line options', link: '/reference/command-line' },
        { text: 'Keyboard & mouse', link: '/reference/keyboard-shortcuts' },
        { text: 'Startup details', link: '/reference/startup' },
        { text: 'Configuration lookup details', link: '/reference/configuration-details' },
        { text: 'Download the manuals', link: '/reference/manuals' }
      ]},
      { text: 'PROJECT & DEVELOPMENT', collapsed: true, items: [
        { text: 'Release notes', link: '/project/release-notes' },
        { text: 'Differences from Motif', link: '/understand/legacy-differences' },
        { text: 'Qt behavior in detail', link: '/understand/qt-behavior' },
        { text: 'Maintain these docs', link: '/develop/documentation' },
        { text: 'Authors', link: '/project/authors' },
        { text: 'License', link: '/project/license' }
      ]}
    ]
  }
})
