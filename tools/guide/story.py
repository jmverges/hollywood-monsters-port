#!/usr/bin/env python3
"""Write hm_story.txt, the story guide the port's menu shows (platform/win32/menu.c), into the game's data directory.

usage: story.py <data-dir> [page.html]

The text is FX Interactive's walkthrough written as a story, as published at nosolobits.com. It is not part of
this repository: this script downloads the page (or reads a saved copy of it) and writes one chapter after
another, each starting with a "== title" line, paragraphs separated by blank lines.
"""
import html.parser
import os
import sys
import urllib.request

URL = 'https://nosolobits.com/es/guia/316/guia-completa-de-hollywood-monsters'


class Guide(html.parser.HTMLParser):
    """The chapters: <section> with an <h2> title, then a "guide-section" with <p> and <li> paragraphs."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.chapters = []          # [title, [paragraphs]]
        self.depth = 0              # <section> nesting inside the current guide-section, 0 = outside
        self.where = None           # 'title' or 'para' while collecting text
        self.buf = []

    def handle_starttag(self, tag, attrs):
        cls = dict(attrs).get('class') or ''
        if tag == 'section' and self.depth:
            self.depth += 1
        elif tag == 'section' and 'guide-section' in cls.split():
            self.depth = 1
        elif tag == 'h2' and not cls:
            self.where, self.buf = 'title', []
        elif tag in ('p', 'li') and self.depth and self.chapters:
            self.where, self.buf = 'para', ['- ' if tag == 'li' else '']
        elif tag == 'br' and self.where == 'para':
            self.buf.append('\n')

    def handle_endtag(self, tag):
        if tag == 'section' and self.depth:
            self.depth -= 1
        elif tag == 'h2' and self.where == 'title':
            title = ' '.join(''.join(self.buf).split())
            if title[:1].isdigit():
                self.chapters.append([title, []])
            self.where = None
        elif tag in ('p', 'li') and self.where == 'para':
            text = '\n'.join(' '.join(line.split()) for line in ''.join(self.buf).split('\n')).strip()
            if text and text not in ('-',):
                self.chapters[-1][1].append(text)
            self.where = None

    def handle_data(self, data):
        if self.where:
            self.buf.append(data)


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    data = sys.argv[1]
    if len(sys.argv) == 3:
        page = open(sys.argv[2], encoding='utf-8').read()
    else:
        req = urllib.request.Request(URL, headers={'User-Agent': 'Mozilla/5.0'})
        page = urllib.request.urlopen(req, timeout=30).read().decode('utf-8')
    g = Guide()
    g.feed(page)
    if len(g.chapters) < 10:
        sys.exit('only %d chapters found: the page has changed' % len(g.chapters))
    out = ['# Hollywood Monsters: the story, from FX Interactive\'s guide as published at', '# ' + URL, '']
    for title, paras in g.chapters:
        out += ['== ' + title, '']
        for p in paras:
            out += [p, '']
    dst = os.path.join(data, 'hm_story.txt')
    with open(dst, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))
    print('%s: %d chapters' % (dst, len(g.chapters)))


if __name__ == '__main__':
    main()
