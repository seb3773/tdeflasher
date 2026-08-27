#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# Script de mise à jour du Dépôt APT tdeFlasher & GitHub Pages
# ==============================================================================

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
PAGES_BRANCH="gh-pages"

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "Error: missing required command: $1" >&2
        exit 1
    }
}

need_cmd dpkg-scanpackages
need_cmd apt-ftparchive
need_cmd git

echo "=================================================="
echo " tdeFlasher APT Repository & GitHub Pages Sync"
echo "=================================================="

# Ensure we have deb, qsi, and appimage packages
DEB_FILES=($(find "$REPO_DIR" -maxdepth 1 -name "tdeflasher_*_amd64.deb" | sort -V -r))
if [ ${#DEB_FILES[@]} -eq 0 ]; then
    echo "Error: No tdeflasher_*_amd64.deb package found in $REPO_DIR." >&2
    echo "Please run ./create_deb.sh first." >&2
    exit 1
fi

QSI_FILES=($(find "$REPO_DIR" -maxdepth 1 -name "setup_tdeflasher_*.qsi" | sort -V -r))
APPIMAGE_FILES=($(find "$REPO_DIR" -maxdepth 1 -name "tdeflasher-*.AppImage" | sort -V -r))

PAGES_DIR=$(mktemp -d -t tdeflasher-gh-pages-XXXXXX)
echo "Staging in temporary directory: $PAGES_DIR"

REMOTE_URL="$(git -C "$REPO_DIR" remote get-url origin)"

# Clone or checkout gh-pages into temp directory
git clone --branch "$PAGES_BRANCH" --single-branch "$REMOTE_URL" "$PAGES_DIR" 2>/dev/null || {
    echo "Creating new orphan gh-pages branch in temp directory..."
    git init "$PAGES_DIR"
    (
        cd "$PAGES_DIR"
        git checkout --orphan "$PAGES_BRANCH"
        git remote add origin "$REMOTE_URL"
    )
}

# Structure pool and dists directories for standard APT repo
POOL_DIR="$PAGES_DIR/pool/main/t/tdeflasher"
DISTS_DIR="$PAGES_DIR/dists/stable/main/binary-amd64"
mkdir -p "$POOL_DIR"
mkdir -p "$DISTS_DIR"

# Copy all deb packages into pool
for deb in "${DEB_FILES[@]}"; do
    echo "  -> Added DEB: $(basename "$deb")"
    cp -a "$deb" "$POOL_DIR/"
done

# Copy latest QSI and AppImage to root of pages
for qsi in "${QSI_FILES[@]}"; do
    echo "  -> Added QSI: $(basename "$qsi")"
    cp -a "$qsi" "$PAGES_DIR/"
done
for appimg in "${APPIMAGE_FILES[@]}"; do
    echo "  -> Added AppImage: $(basename "$appimg")"
    cp -a "$appimg" "$PAGES_DIR/"
done

# Generate Packages & Packages.gz index files
echo "Generating Packages index..."
(
    cd "$PAGES_DIR"
    dpkg-scanpackages --multiversion --arch amd64 pool/main > "$DISTS_DIR/Packages"
    gzip -9 -c "$DISTS_DIR/Packages" > "$DISTS_DIR/Packages.gz"
)

# Generate Release file
echo "Generating Release manifest..."
apt-ftparchive \
  -o APT::FTPArchive::Release::Origin="tdeFlasher" \
  -o APT::FTPArchive::Release::Label="tdeFlasher APT Repository" \
  -o APT::FTPArchive::Release::Suite="stable" \
  -o APT::FTPArchive::Release::Codename="stable" \
  -o APT::FTPArchive::Release::Architectures="amd64" \
  -o APT::FTPArchive::Release::Components="main" \
  -o APT::FTPArchive::Release::Description="APT Repository for tdeFlasher (OS Image Flasher for Trinity Desktop & Linux)" \
  release "$PAGES_DIR/dists/stable" > "$PAGES_DIR/dists/stable/Release"

# Copy assets (logo, favicon, screenshots, etc.)
if [ -f "$REPO_DIR/konquiflasher.png" ]; then
    cp -a "$REPO_DIR/konquiflasher.png" "$PAGES_DIR/"
fi
if [ -f "$REPO_DIR/gui/logo.png" ]; then
    cp -a "$REPO_DIR/gui/logo.png" "$PAGES_DIR/favicon.png"
fi
if [ -d "$REPO_DIR/screenshots" ]; then
    cp -a "$REPO_DIR/screenshots" "$PAGES_DIR/"
fi

# Create .nojekyll to prevent GitHub Pages Jekyll processing
touch "$PAGES_DIR/.nojekyll"

# Determine latest version and file names
LATEST_VERSION="1.0.0"
if [ -f "$REPO_DIR/create_deb.sh" ]; then
    V_TMP=$(grep '^VERSION=' "$REPO_DIR/create_deb.sh" | head -n1 | cut -d'"' -f2 || true)
    if [ -n "$V_TMP" ]; then
        LATEST_VERSION="$V_TMP"
    fi
fi

# Find standard dynamic deb
LATEST_DEB_NAME=""
for deb in "${DEB_FILES[@]}"; do
    base=$(basename "$deb")
    if [[ "$base" != *"_static_"* ]]; then
        LATEST_DEB_NAME="$base"
        break
    fi
done
if [ -z "$LATEST_DEB_NAME" ] && [ ${#DEB_FILES[@]} -gt 0 ]; then
    LATEST_DEB_NAME=$(basename "${DEB_FILES[0]}")
fi

# Find static deb
LATEST_STATIC_DEB_NAME=""
for deb in "${DEB_FILES[@]}"; do
    base=$(basename "$deb")
    if [[ "$base" == *"_static_"* ]]; then
        LATEST_STATIC_DEB_NAME="$base"
        break
    fi
done

LATEST_QSI_NAME=""
if [ ${#QSI_FILES[@]} -gt 0 ]; then
    LATEST_QSI_NAME=$(basename "${QSI_FILES[0]}")
fi
LATEST_APPIMAGE_NAME=""
if [ ${#APPIMAGE_FILES[@]} -gt 0 ]; then
    LATEST_APPIMAGE_NAME=$(basename "${APPIMAGE_FILES[0]}")
fi

# Collect screenshots (max 8)
SCREENSHOT_ITEMS=""
if [ -d "$REPO_DIR/screenshots" ]; then
    SCREENSHOT_FILES=($(find "$REPO_DIR/screenshots" -maxdepth 1 -type f \( -name "*.jpg" -o -name "*.png" \) | sort -V | head -n 8))
    for ss in "${SCREENSHOT_FILES[@]}"; do
        ss_name=$(basename "$ss")
        SCREENSHOT_ITEMS="${SCREENSHOT_ITEMS}        <div class=\"screenshot-thumb\" onclick=\"openModal('screenshots/${ss_name}')\">
          <img src=\"screenshots/${ss_name}\" alt=\"${ss_name}\">
        </div>
"
    done
fi

# Build Method 2 Download cards (all 4 packages)
STATIC_DEB_CARD=""
if [ -n "$LATEST_STATIC_DEB_NAME" ]; then
    STATIC_DEB_CARD="
        <div class=\"download-card\">
          <div class=\"download-header\">
            <span class=\"download-title\">Standalone (.deb)</span>
            <span class=\"download-tag\">Static TQt3</span>
          </div>
          <p class=\"download-desc\">Debian package with embedded static TQt3 framework (runs on Debian/Ubuntu/Mint without TDE installed).</p>
          <a href=\"pool/main/t/tdeflasher/${LATEST_STATIC_DEB_NAME}\" class=\"btn-download\">
            Download Static .deb
          </a>
        </div>"
fi

cat << EOF > "$PAGES_DIR/index.html"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>tdeFlasher v${LATEST_VERSION} - APT Repository</title>
  <link rel="icon" type="image/png" href="favicon.png">
  <meta name="description" content="Official APT Repository and download portal for tdeFlasher - Lightweight, high-performance OS image flasher natively designed for Trinity Desktop Environment (TDE) & Linux.">
  <style>
    :root {
      --bg: #12141a;
      --card-bg: #1c1f2b;
      --card-hover: #222738;
      --accent: #38bdf8;
      --accent-grad: linear-gradient(135deg, #0284c7, #38bdf8);
      --text: #e2e8f0;
      --text-muted: #94a3b8;
      --code-bg: #0f1117;
      --border: #2e364f;
      --radius: 12px;
      --radius-sm: 8px;
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: var(--bg);
      color: var(--text);
      line-height: 1.6;
      padding: 40px 20px;
    }

    .container {
      max-width: 840px;
      margin: 0 auto;
    }

    header {
      text-align: center;
      margin-bottom: 40px;
    }

    .logo {
      width: 110px;
      height: 110px;
      margin-bottom: 16px;
      filter: drop-shadow(0 8px 24px rgba(56, 189, 248, 0.4));
      border-radius: 18px;
      transition: transform 0.3s cubic-bezier(0.34, 1.56, 0.64, 1);
    }

    .logo:hover {
      transform: scale(1.08) rotate(3deg);
    }

    .badge-group {
      display: flex;
      justify-content: center;
      gap: 10px;
      margin-bottom: 12px;
      flex-wrap: wrap;
    }

    .badge {
      display: inline-block;
      padding: 4px 14px;
      font-size: 0.85rem;
      font-weight: 600;
      color: #fff;
      background: var(--accent-grad);
      border-radius: 20px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }

    .badge-green {
      background: linear-gradient(135deg, #15803d, #22c55e);
    }

    .badge-purple {
      background: linear-gradient(135deg, #6366f1, #a855f7);
    }

    .badge-amber {
      background: linear-gradient(135deg, #b45309, #f59e0b);
    }

    .version-pill {
      display: inline-block;
      font-size: 1.1rem;
      font-weight: 600;
      color: #38bdf8;
      background: rgba(56, 189, 248, 0.12);
      border: 1px solid rgba(56, 189, 248, 0.35);
      padding: 2px 12px;
      border-radius: 20px;
      vertical-align: middle;
      margin-left: 8px;
    }

    h1 {
      font-size: 2.4rem;
      font-weight: 700;
      margin-bottom: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    p.lead {
      font-size: 1.1rem;
      color: var(--text-muted);
      max-width: 680px;
      margin: 0 auto;
    }

    .card {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: 24px;
      margin-bottom: 24px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
    }

    h2 {
      font-size: 1.3rem;
      margin-bottom: 14px;
      display: flex;
      align-items: center;
      gap: 10px;
      color: #fff;
    }

    /* Terminal & Code snippet box */
    .code-container {
      position: relative;
      margin-top: 10px;
    }

    pre {
      background: var(--code-bg);
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      padding: 16px;
      padding-right: 80px;
      overflow-x: auto;
      font-family: "Courier New", Courier, monospace;
      font-size: 0.92rem;
      color: #38bdf8;
      line-height: 1.6;
    }

    .copy-btn {
      position: absolute;
      top: 12px;
      right: 12px;
      background: rgba(255, 255, 255, 0.08);
      border: 1px solid rgba(255, 255, 255, 0.18);
      color: var(--text);
      padding: 5px 12px;
      border-radius: 6px;
      font-size: 0.8rem;
      cursor: pointer;
      transition: all 0.2s;
    }

    .copy-btn:hover {
      background: var(--accent);
      color: #fff;
      border-color: var(--accent);
    }

    /* Downloads Grid */
    .downloads-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(185px, 1fr));
      gap: 16px;
      margin-top: 16px;
    }

    .download-card {
      background: #141722;
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      padding: 18px;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      transition: all 0.2s ease;
    }

    .download-card:hover {
      transform: translateY(-2px);
      border-color: #38bdf8;
      background: var(--card-hover);
    }

    .download-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 8px;
    }

    .download-title {
      font-size: 1.02rem;
      font-weight: 700;
      color: #fff;
    }

    .download-tag {
      font-size: 0.70rem;
      font-weight: 600;
      padding: 2px 7px;
      border-radius: 12px;
      background: rgba(56, 189, 248, 0.15);
      color: #38bdf8;
      border: 1px solid rgba(56, 189, 248, 0.3);
      white-space: nowrap;
    }

    .download-desc {
      font-size: 0.84rem;
      color: var(--text-muted);
      margin-bottom: 14px;
      flex-grow: 1;
      line-height: 1.45;
    }

    .btn-download {
      background: #1e293b;
      color: #38bdf8;
      border: 1px solid #334155;
      padding: 8px 14px;
      border-radius: 6px;
      text-align: center;
      text-decoration: none;
      font-weight: 600;
      font-size: 0.88rem;
      transition: all 0.2s;
    }

    .btn-download:hover {
      background: var(--accent);
      color: #ffffff;
      border-color: var(--accent);
    }

    .features-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
      gap: 16px;
      margin-top: 14px;
    }

    .feature-item {
      background: #141722;
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      padding: 16px;
    }

    .feature-icon {
      font-size: 1.4rem;
      margin-bottom: 6px;
      display: inline-block;
    }

    .feature-title {
      font-size: 0.98rem;
      font-weight: 700;
      color: #ffffff;
      margin-bottom: 4px;
    }

    .feature-text {
      font-size: 0.85rem;
      color: var(--text-muted);
      line-height: 1.45;
    }

    /* Screenshots gallery */
    .screenshots-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 12px;
      margin-top: 14px;
    }

    .screenshot-thumb {
      border-radius: var(--radius-sm);
      overflow: hidden;
      border: 1px solid var(--border);
      cursor: pointer;
      aspect-ratio: 16 / 10;
      background: #0f1117;
      transition: all 0.2s ease;
    }

    .screenshot-thumb:hover {
      transform: scale(1.03);
      border-color: #38bdf8;
      box-shadow: 0 6px 20px rgba(56, 189, 248, 0.25);
    }

    .screenshot-thumb img {
      width: 100%;
      height: 100%;
      object-fit: cover;
      display: block;
    }

    /* Modal / Lightbox */
    .modal {
      display: none;
      position: fixed;
      z-index: 1000;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(0, 0, 0, 0.85);
      backdrop-filter: blur(6px);
      align-items: center;
      justify-content: center;
      padding: 20px;
    }

    .modal.active {
      display: flex;
    }

    .modal img {
      max-width: 90vw;
      max-height: 88vh;
      border-radius: 8px;
      border: 1px solid var(--border);
      box-shadow: 0 12px 40px rgba(0, 0, 0, 0.6);
    }

    .modal-close {
      position: absolute;
      top: 20px;
      right: 30px;
      font-size: 2.2rem;
      color: #fff;
      cursor: pointer;
      line-height: 1;
    }

    footer {
      text-align: center;
      margin-top: 45px;
      font-size: 0.9rem;
      color: var(--text-muted);
    }

    footer a {
      color: var(--accent);
      text-decoration: none;
    }

    footer a:hover {
      text-decoration: underline;
    }

    .footer-links {
      margin-top: 8px;
      font-size: 0.82rem;
      color: #64748b;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <img src="konquiflasher.png" alt="tdeFlasher Logo" class="logo">
      <div class="badge-group">
        <div class="badge">Official APT Repository</div>
        <div class="badge badge-green">TDE &amp; Linux Native</div>
        <div class="badge badge-purple">x86_64</div>
        <div class="badge badge-amber">No-Electron Fast C++</div>
      </div>
      <h1>tdeFlasher <span class="version-pill">v${LATEST_VERSION}</span></h1>
      <p class="lead">Lightweight, blazing-fast OS image flasher natively designed with TQt3 for Trinity Desktop Environment (TDE) and Linux systems.</p>
    </header>

    <!-- Method 1: APT Repository -->
    <div class="card">
      <h2>
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#38bdf8" stroke-width="2"><path d="M4 17l6-6-6-6M12 19h8"/></svg>
        Method 1: Add the APT Repository (Recommended)
      </h2>
      <p style="color: var(--text-muted); font-size: 0.95rem;">
        Add the official repository to your system to receive regular automated updates via <code>apt</code>:
      </p>

      <div class="code-container">
        <pre id="apt-code"><code>echo "deb [trusted=yes] https://seb3773.github.io/tdeflasher/ stable main" | sudo tee /etc/apt/sources.list.d/tdeflasher.list
sudo apt update
sudo apt install tdeflasher</code></pre>
        <button class="copy-btn" onclick="copyCode('apt-code', this)">Copy</button>
      </div>

      <p style="color: var(--text-muted); font-size: 0.85rem; margin-top: 14px;">
        Compatible with Q4OS, Debian, Devuan, Ubuntu, Linux Mint and all Debian-based distributions.
      </p>
    </div>

    <!-- Method 2: Direct Packages -->
    <div class="card">
      <h2>
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#38bdf8" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/></svg>
        Method 2: Direct Package Download (.deb / .qsi / .AppImage)
      </h2>
      <p style="color: var(--text-muted); font-size: 0.95rem;">
        Choose the package format best suited for your distribution:
      </p>

      <div class="downloads-grid">
        <div class="download-card">
          <div class="download-header">
            <span class="download-title">Debian / TDE (.deb)</span>
            <span class="download-tag">Recommended</span>
          </div>
          <p class="download-desc">Standard dynamically linked package for Trinity Desktop / Debian-based systems.</p>
          <a href="pool/main/t/tdeflasher/${LATEST_DEB_NAME}" class="btn-download">
            Download .deb
          </a>
        </div>
${STATIC_DEB_CARD}
        <div class="download-card">
          <div class="download-header">
            <span class="download-title">Q4OS Installer (.qsi)</span>
            <span class="download-tag">Q4OS 1-Click</span>
          </div>
          <p class="download-desc">Graphical one-click installer designed specifically for Q4OS Trinity desktop.</p>
          <a href="${LATEST_QSI_NAME}" class="btn-download">
            Download .qsi
          </a>
        </div>

        <div class="download-card">
          <div class="download-header">
            <span class="download-title">Standalone (.AppImage)</span>
            <span class="download-tag">Universal x86_64</span>
          </div>
          <p class="download-desc">Self-contained portable executable with bundled dependencies (runs on any Linux distro).</p>
          <a href="${LATEST_APPIMAGE_NAME}" class="btn-download">
            Download AppImage
          </a>
        </div>
      </div>
      <p style="color: var(--text-muted); font-size: 0.85rem; margin-top: 16px;">
        * Note: The Q4OS installer (.qsi) automatically configures the APT repository during installation for future updates.
      </p>
    </div>

    <!-- Key Features -->
    <div class="card">
      <h2>
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#38bdf8" stroke-width="2"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg>
        Key Capabilities &amp; Architecture
      </h2>

      <div class="features-grid">
        <div class="feature-item">
          <span class="feature-icon">⚡</span>
          <div class="feature-title">Iconic 3-Step Wizard</div>
          <div class="feature-text">Select Image &rarr; Select Drive &rarr; Flash! Intuitive, streamlined, and foolproof workflow replicating Etcher without the Electron overhead.</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">🌐</span>
          <div class="feature-title">Flash Directly from URL</div>
          <div class="feature-text">Stream and write raw or compressed OS images straight from remote HTTP/HTTPS servers to your USB drive without saving to disk first.</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">📦</span>
          <div class="feature-title">Universal Image Support</div>
          <div class="feature-text">Native support for raw <code>.img</code> and <code>.iso</code>, plus on-the-fly streaming decompression for <code>.zip</code>, <code>.xz</code>, <code>.gz</code>, <code>.bz2</code> via libarchive.</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">🔒</span>
          <div class="feature-title">Safe, Verified &amp; O_DIRECT</div>
          <div class="feature-text">High-speed SHA-256 integrity verification, system drive write protection warnings, and unbuffered <code>O_DIRECT</code> I/O to avoid kernel memory exhaustion.</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">💾</span>
          <div class="feature-title">Clone Drive</div>
          <div class="feature-text">Direct block-device to block-device replication with real-time speed/ETA metrics and automatic target capacity validation.</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">🚀</span>
          <div class="feature-title">Native TQt3 / C++ Performance</div>
          <div class="feature-text">Starts instantly, consumes only a few megabytes of RAM, and seamlessly integrates with Trinity Desktop Environment (TDE).</div>
        </div>
      </div>
    </div>

    <!-- Screenshots -->
    <div class="card">
      <h2>
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#38bdf8" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/></svg>
        Screenshots
      </h2>
      <div class="screenshots-grid">
${SCREENSHOT_ITEMS}      </div>
    </div>

    <!-- Footer -->
    <footer>
      <p>Source Code &amp; Releases: <a href="https://github.com/seb3773/tdeflasher" target="_blank" rel="noopener">github.com/seb3773/tdeflasher</a></p>
      <p style="margin-top: 6px;">Developed with ❤️ for the Trinity Desktop Environment (TDE) and Q4OS community.</p>
      <p class="footer-links">
        <a href="http://trinitydesktop.org/" target="_blank" rel="noopener">http://trinitydesktop.org/</a> &bull; 
        <a href="https://www.q4os.org/" target="_blank" rel="noopener">https://www.q4os.org/</a> &bull; 
        <a href="https://www.q4os.org/forum/index.php" target="_blank" rel="noopener">https://www.q4os.org/forum/index.php</a>
      </p>
    </footer>

  </div>

  <!-- Lightbox Modal -->
  <div id="imageModal" class="modal" onclick="closeModal()">
    <span class="modal-close">&times;</span>
    <img id="modalImg" src="" alt="Enlarged screenshot" onclick="event.stopPropagation()">
  </div>

  <script>
    function copyCode(id, btn) {
      const text = document.getElementById(id).innerText;
      navigator.clipboard.writeText(text).then(() => {
        const orig = btn.innerText;
        btn.innerText = "Copied!";
        setTimeout(() => btn.innerText = orig, 2000);
      });
    }

    function openModal(src) {
      document.getElementById('modalImg').src = src;
      document.getElementById('imageModal').classList.add('active');
    }

    function closeModal() {
      document.getElementById('imageModal').classList.remove('active');
    }

    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') closeModal();
    });
  </script>
</body>
</html>
EOF

# Copy index.html back to main repo workspace for offline reference
cp -a "$PAGES_DIR/index.html" "$REPO_DIR/index.html"

# Git commit and push to gh-pages
echo "Committing and pushing to gh-pages branch..."
(
    cd "$PAGES_DIR"
    git add -A
    git commit -m "Update APT repository and deploy GitHub Pages (4 packages): $(date +'%Y-%m-%d %H:%M:%S')" || echo "No changes to commit."
    git push origin "$PAGES_BRANCH"
)

echo "Cleaning up temporary directory..."
rm -rf "$PAGES_DIR"

echo "=================================================="
echo " SUCCESS: APT repository updated on gh-pages!"
echo " URL: https://seb3773.github.io/tdeflasher/"
echo "=================================================="
