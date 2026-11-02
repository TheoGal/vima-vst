() => {
    // paragraphs that are only a bold label ("note mode:") stay with what follows
    for (const p of document.querySelectorAll('p')) {
        const st = p.querySelector(':scope > strong');
        if (st && p.textContent.trim() === st.textContent.trim()) p.classList.add('label');
    }
    // tables stay whole: Chromium ignores break-inside on the table itself, so wrap each one
    for (const t of Array.from(document.querySelectorAll('table'))) {
        const box = document.createElement('div');
        box.className = 'table-box';
        t.parentElement.insertBefore(box, t);
        box.appendChild(t);
    }
    // "2. You can use drag and drop..." starts a new page with its image
    for (const ol of document.querySelectorAll('ol'))
        if (ol.textContent.trim().startsWith('You can use drag and drop')) ol.classList.add('page-break');
    // "The pattern controls" starts a new page with its image
    for (const h of document.querySelectorAll('h3'))
        if (h.textContent.trim() === 'The pattern controls') h.classList.add('page-break');
    // a floated image and the text beside it (up to the <br clear>) stay on one page together
    for (const img of Array.from(document.querySelectorAll('img[align="left"]'))) {
        let start = img;
        while (start.parentElement && start.parentElement !== document.body && start.parentElement.tagName !== 'SECTION') start = start.parentElement;
        const group = [start];
        let n = start.nextElementSibling;
        while (n && ! (n.matches('br[clear]') || n.querySelector('br[clear]'))) { group.push(n); n = n.nextElementSibling; }
        if (n) group.push(n);
        const box = document.createElement('div');
        box.className = 'keep';
        start.parentElement.insertBefore(box, start);
        for (const el of group) box.appendChild(el);
    }
    // tall centered screenshots: at most 85 mm high, keeping their proportions (not on the cover)
    const maxH = 85 * 96 / 25.4;
    for (const img of document.querySelectorAll('p[align="center"] img')) {
        if (img.closest('.cover')) continue;
        const r = img.getBoundingClientRect();
        if (r.height > maxH) img.style.width = (r.width * maxH / r.height) + 'px';
    }
    // attach each caption to its image: same width, directly underneath
    for (const p of document.querySelectorAll('p[align="center"]')) {
        const img = p.querySelector('img'), em = p.querySelector('em');
        if (!img || !em) continue;
        p.classList.add('captioned');
        em.style.width = img.getBoundingClientRect().width + 'px';
    }
}
