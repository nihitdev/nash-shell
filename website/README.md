# Nash documentation website

A dependency-light static site with an original Arch-inspired layout. No accounts, analytics,
third-party fonts, framework runtime or visitor data collection. Documentation pages are generated
from the repository's Markdown handbook, so source and website stay together.

```sh
python3 -m venv website/.venv
website/.venv/bin/pip install -r website/requirements.txt
website/.venv/bin/python website/tools/build.py
python3 -m http.server 4173 --directory website/dist
```

The builder copies current handbook sources to `content/` as a self-contained snapshot.
When the parent handbook is unavailable, it builds from that snapshot. Generated `dist/` is
not committed. Deployment is manual; no hosting account is linked by this repository.

Features: readable multi-page docs, client-side full-text search, page outlines, keyboard search,
dark/light themes, copy buttons, tool filtering and a local configuration builder. The builder
only produces config text; it does not read or change a visitor's machine.

Run static validation with `python3 website/tools/check.py`. CI builds the site and verifies
all internal links, anchors, assets, search targets and JavaScript syntax.

Optional browser QA (with the local server running):

```sh
cd website
npm ci
npx playwright install chromium
npm run test:browser
```

Set `NASH_CHROME` to use an installed Chrome executable, or `NASH_SITE_URL` to change the local
base URL. Browser tests cover responsive overflow, search, filters, config generation/download,
navigation and theme persistence. Screenshots default to temporary files; they are not shipped.

## Manual Vercel deployment

Import `nihitdev/nash-shell` into your own Vercel account and select:

| Setting | Value |
|---|---|
| Root directory | `website` |
| Framework preset | Other |
| Install command | `python3 -m pip install --target .python-deps -r requirements.txt` |
| Build command | `PYTHONPATH=.python-deps python3 tools/build.py && python3 tools/check.py` |
| Output directory | `dist` |

These values are also recorded in `website/vercel.json`. No environment variables, accounts,
databases, runtime server, or secret keys are required by the site. Playwright is optional local
QA tooling and is not installed by the production build command. If you restrict Vercel's root
access to `website`, the retained handbook snapshot still allows the build to succeed.

The site is multi-page static HTML. Keep the `.html` URLs and directory index pages; do not
add a catch-all SPA rewrite. Vercel can serve the generated `404.html` for missing pages.
Review the build logs in your Vercel project before assigning a custom domain. No deployment
has been performed as part of preparing these files.

References: [Vercel build settings](https://vercel.com/docs/builds/configure-a-build),
[vercel.json](https://vercel.com/docs/project-configuration/vercel-json).
