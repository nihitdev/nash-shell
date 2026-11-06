#!/usr/bin/env python3
"""Validate every generated internal link, anchor, and search entry."""
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urlsplit,unquote
import json
ROOT=Path(__file__).resolve().parents[1]/'dist'
class Page(HTMLParser):
 def __init__(self):super().__init__();self.links=[];self.ids=set();self.titles=0
 def handle_starttag(self,tag,attrs):
  a=dict(attrs)
  if 'id' in a:self.ids.add(a['id'])
  if tag=='title':self.titles+=1
  if tag in ['a','link','script','img']:
   target=a.get('href') or a.get('src')
   if target:self.links.append(target)
pages={}
for file in ROOT.rglob('*.html'):
 page=Page();page.feed(file.read_text());assert page.titles==1,file;pages[file.resolve()]=page
errors=[]
for file,page in pages.items():
 for href in page.links:
  url=urlsplit(href)
  if url.scheme or url.netloc:continue
  target=(ROOT/unquote(url.path).lstrip('/')) if url.path.startswith('/') else file.parent/unquote(url.path)
  if not url.path:target=file
  if target.is_dir():target=target/'index.html'
  target=target.resolve()
  if not target.exists():errors.append(f'{file.name}: missing {href}')
  elif url.fragment and target in pages and unquote(url.fragment) not in pages[target].ids:errors.append(f'{file.name}: missing anchor {href}')
for entry in json.loads((ROOT/'search-index.json').read_text()):
 assert (ROOT/entry['url'].lstrip('/')).exists(),entry['url']
assert len(pages)>=25
if errors:raise SystemExit('\n'.join(errors))
print(f'Validated {len(pages)} pages: titles, internal links, anchors, assets and search targets.')
