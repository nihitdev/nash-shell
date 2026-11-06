'use strict';
(() => {
  const $ = (s, root = document) => root.querySelector(s);
  const $$ = (s, root = document) => [...root.querySelectorAll(s)];
  let toastTimer;
  const toast = text => { const el = $('#toast'); el.textContent = text; el.classList.add('visible'); clearTimeout(toastTimer); toastTimer = setTimeout(() => el.classList.remove('visible'), 2200); };
  try { const saved = localStorage.getItem('nash-theme'); if (saved === 'dark' || saved === 'light') document.documentElement.dataset.theme = saved; } catch {}
  $('.theme-toggle')?.addEventListener('click', () => { const theme = document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark'; document.documentElement.dataset.theme = theme; try { localStorage.setItem('nash-theme', theme); } catch {} });
  $('.mobile-toggle')?.addEventListener('click', event => { const open = $('.top-nav').classList.toggle('open'); event.currentTarget.setAttribute('aria-expanded', String(open)); });
  async function copy(text) {
    if (navigator.clipboard && window.isSecureContext) { try { await navigator.clipboard.writeText(text); toast('Copied to clipboard'); return true; } catch {} }
    const box = document.createElement('textarea'); box.value = text; box.style.position = 'fixed'; box.style.opacity = '0'; document.body.append(box); box.select(); const copied = document.execCommand('copy'); box.remove(); toast(copied ? 'Copied to clipboard' : 'Copy unavailable — select and copy the text'); return copied;
  }
  $$('pre').forEach(pre => { const code = $('code', pre); if (!code || code.id === 'config-output') return; const button = document.createElement('button'); button.className = 'copy-code'; button.textContent = 'Copy'; button.setAttribute('aria-label', 'Copy code'); button.addEventListener('click', async () => { if (await copy(code.textContent)) { button.textContent = 'Copied'; setTimeout(() => button.textContent = 'Copy', 1700); } }); pre.append(button); });
  const dialog = $('.search-dialog'); const input = $('#search-input'); const results = $('#search-results'); let indexPromise; let focusedResult = -1;
  async function searchIndex() { if (!indexPromise) indexPromise = fetch('/search-index.json').then(response => { if (!response.ok) throw new Error('Search index unavailable'); return response.json(); }).catch(error => { indexPromise = null; throw error; }); return indexPromise; }
  function closeSearch() { dialog.close(); }
  function openSearch() { if (!dialog.open) dialog.showModal(); input.focus(); searchIndex().catch(() => {}); }
  $$('[data-search-open]').forEach(button => button.addEventListener('click', openSearch));
  $('[data-search-close]')?.addEventListener('click', closeSearch);
  dialog?.addEventListener('click', event => { if (event.target === dialog) closeSearch(); });
  document.addEventListener('keydown', event => { const editing = /INPUT|TEXTAREA|SELECT/.test(document.activeElement?.tagName) || document.activeElement?.isContentEditable; if (event.key === '/' && !editing || (event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'k') { event.preventDefault(); openSearch(); } if (event.key === 'Escape' && dialog.open) closeSearch(); });
  let queryVersion = 0;
  input?.addEventListener('input', async () => {
    const version = ++queryVersion; const query = input.value.trim().toLowerCase(); focusedResult = -1; results.replaceChildren();
    if (!query) { const p = document.createElement('p'); p.className = 'search-hint'; p.textContent = 'Try “fastfetch”, “aliases”, or “prompt.engine”.'; results.append(p); return; }
    try {
      const index = await searchIndex(); if (version !== queryVersion) return;
      const terms = query.split(/\s+/); const matches = index.map(page => { const title = page.title.toLowerCase(); const text = page.text.toLowerCase(); const score = terms.reduce((n, term) => n + (title.includes(term) ? 20 : 0) + (text.includes(term) ? 1 : 0), 0); return { ...page, score, matches: terms.every(term => (title + ' ' + text).includes(term)) }; }).filter(page => page.matches).sort((a, b) => b.score - a.score).slice(0, 9);
      if (!matches.length) { const p = document.createElement('p'); p.className = 'search-hint'; p.textContent = 'No matching guides. Try a tool name or a shorter search.'; results.append(p); }
      for (const page of matches) { const link = document.createElement('a'); link.className = 'search-result'; link.href = page.url; const category = document.createElement('span'); category.className = 'result-category'; category.textContent = page.category; const title = document.createElement('strong'); title.textContent = page.title; const excerpt = document.createElement('p'); const position = Math.max(0, page.text.toLowerCase().indexOf(terms[0]) - 55); excerpt.textContent = (position ? '…' : '') + page.text.slice(position, position + 170).replace(/\s+/g, ' '); link.append(category, title, excerpt); results.append(link); }
    } catch { const p = document.createElement('p'); p.className = 'search-hint'; p.textContent = 'Search could not load. Use the handbook navigation or try again.'; results.append(p); }
  });
  dialog?.addEventListener('keydown', event => { const links = $$('.search-result', results); if (!links.length) return; if (event.key === 'ArrowDown' || event.key === 'ArrowUp') { event.preventDefault(); focusedResult = (focusedResult + (event.key === 'ArrowDown' ? 1 : -1) + links.length) % links.length; links.forEach((link, n) => link.classList.toggle('highlighted', n === focusedResult)); links[focusedResult].scrollIntoView({ block: 'nearest' }); } if (event.key === 'Enter') { const selected = links[Math.max(0, focusedResult)]; if (selected) { event.preventDefault(); location.assign(selected.href); } } });
  const toolFilter = $('#tool-filter'); let toolCategory = 'all';
  function filterTools() { const query = toolFilter.value.trim().toLowerCase(); let count = 0; $$('[data-tool]').forEach(card => { const visible = card.dataset.tool.includes(query) && (toolCategory === 'all' || card.dataset.kind === toolCategory); card.hidden = !visible; if (visible) count++; }); $('#tool-count').textContent = `${count} ${count === 1 ? 'tool' : 'tools'}`; $('#tool-empty').hidden = count !== 0; }
  toolFilter?.addEventListener('input', filterTools); $$('[data-tool-category]').forEach(button => button.addEventListener('click', () => { toolCategory = button.dataset.toolCategory; $$('[data-tool-category]').forEach(other => { other.classList.toggle('selected', other === button); other.setAttribute('aria-pressed', String(other === button)); }); filterTools(); }));
  const form = $('#config-form');
  function configText() { const values = new FormData(form); return ['# ~/.config/nash/config.nash', '# Generated locally. Edit freely.', '', `config set prompt.engine ${values.get('engine') === 'starship' ? 'starship' : 'native'}`, '', '# Optional native tool support', ...['fastfetch','eza','bat'].map(name => `config set integrations.${name}.enabled ${values.has(name)}`), '', '# Interactive editing', `config set autosuggestions.enabled ${values.has('suggestions')}`, `config set highlighting.enabled ${values.has('highlighting')}`, `config set completion.enabled ${values.has('completion')}`, '', "alias gs='git status --short'", 'config bind Ctrl-F accept-suggestion', 'config bind Ctrl-R history-search', ''].join('\n'); }
  if (form) { const update = () => $('#config-output').textContent = configText(); form.addEventListener('change', update); update(); $('#copy-config').addEventListener('click', () => copy(configText())); $('#download-config').addEventListener('click', () => { const url = URL.createObjectURL(new Blob([configText()], { type: 'text/plain' })); const link = document.createElement('a'); link.href = url; link.download = 'config.nash'; link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000); toast('Config downloaded'); }); }
})();
