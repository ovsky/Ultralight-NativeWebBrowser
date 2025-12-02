(function () {
    const state = {
        snapshot: null,
        flat: [],
        filter: '',
        folders: [],
        editingBookmarkId: null,
        editingBookmarkParent: null,
        editingFolderId: null,
        editingFolderParent: null
    };

    function init() {
        bindEvents();
        waitForNative();
    }

    function waitForNative() {
        if (typeof GetBookmarksSnapshot === 'function') {
            refreshAll();
            return;
        }
        setTimeout(waitForNative, 80);
    }

    function refreshAll() {
        refreshSnapshot();
        refreshFolders();
    }

    function refreshSnapshot() {
        if (typeof GetBookmarksSnapshot !== 'function')
            return;
        try {
            const raw = GetBookmarksSnapshot();
            state.snapshot = JSON.parse(raw || '{}');
            buildFlatList();
            renderTrees();
            renderTable();
        } catch (e) {
            console.error('Failed to parse bookmarks snapshot', e);
        }
    }

    function refreshFolders() {
        if (typeof GetBookmarkFolders !== 'function')
            return;
        try {
            const raw = GetBookmarkFolders();
            state.folders = JSON.parse(raw || '[]');
            populateFolderSelect('create-folder');
            populateFolderSelect('folder-parent');
        } catch (e) {
            state.folders = [];
        }
    }

    function populateFolderSelect(id) {
        const select = document.getElementById(id);
        if (!select)
            return;
        select.innerHTML = '';
        state.folders.forEach(folder => {
            const option = document.createElement('option');
            option.value = folder.id;
            option.textContent = folder.path || folder.title || 'Folder';
            select.appendChild(option);
        });
        if (!select.value && select.options.length)
            select.selectedIndex = 0;
    }

    function buildFlatList() {
        state.flat = [];
        if (state.snapshot && state.snapshot.bar)
            flattenNode(state.snapshot.bar, ['Bookmarks bar']);
        if (state.snapshot && state.snapshot.other)
            flattenNode(state.snapshot.other, ['Other bookmarks']);
    }

    function flattenNode(node, ancestors) {
        if (!node)
            return;
        const pathParts = [...ancestors, node.title || 'Untitled'];
        if (node.type === 'bookmark') {
            state.flat.push({
                id: node.id,
                title: node.title || node.url || 'Untitled',
                url: node.url || '',
                path: pathParts.join(' / '),
                parentId: node.parentId || 0,
                dateAdded: node.dateAdded || 0,
                type: 'bookmark'
            });
        }
        if (Array.isArray(node.children)) {
            node.children.forEach(child => flattenNode(child, pathParts));
        }
    }

    function renderTrees() {
        renderTree(state.snapshot ? state.snapshot.bar : null, document.getElementById('tree-bar'));
        renderTree(state.snapshot ? state.snapshot.other : null, document.getElementById('tree-other'));
    }

    function renderTree(root, host) {
        if (!host)
            return;
        host.innerHTML = '';
        if (!root || !Array.isArray(root.children) || !root.children.length) {
            const empty = document.createElement('li');
            empty.className = 'empty-state';
            empty.textContent = 'Empty';
            host.appendChild(empty);
            return;
        }
        const frag = document.createDocumentFragment();
        root.children.forEach(child => frag.appendChild(createTreeNode(child)));
        host.appendChild(frag);
    }

    function createTreeNode(node) {
        const li = document.createElement('li');
        const row = document.createElement('div');
        row.className = 'node-row';
        const label = document.createElement('div');
        label.className = 'node-label';
        const title = document.createElement('div');
        title.className = 'node-title';
        title.textContent = node.title || (node.url || 'Untitled');
        const meta = document.createElement('div');
        meta.className = 'node-meta';
        meta.textContent = node.type === 'folder' ? 'Folder' : node.url;
        label.appendChild(title);
        label.appendChild(meta);
        const actions = document.createElement('div');
        actions.className = 'node-actions';
        if (node.type === 'bookmark') {
            actions.appendChild(makeActionButton('Open', () => openBookmark(node.url, false)));
            actions.appendChild(makeActionButton('New tab', () => openBookmark(node.url, true)));
            actions.appendChild(makeActionButton('Edit', () => startBookmarkEdit(node)));
            actions.appendChild(makeActionButton('Delete', () => deleteNode(node.id)));
        } else {
            actions.appendChild(makeActionButton('Add page', () => startBookmarkCreate(node.id)));
            actions.appendChild(makeActionButton('Add folder', () => startFolderCreate(node.id)));
            actions.appendChild(makeActionButton('Rename', () => startFolderEdit(node)));
            actions.appendChild(makeActionButton('Delete', () => deleteNode(node.id)));
        }
        row.appendChild(label);
        row.appendChild(actions);
        li.appendChild(row);
        if (node.type === 'folder' && Array.isArray(node.children) && node.children.length) {
            const sub = document.createElement('ul');
            sub.className = 'tree';
            node.children.forEach(child => sub.appendChild(createTreeNode(child)));
            li.appendChild(sub);
        }
        return li;
    }

    function makeActionButton(text, handler) {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.textContent = text;
        btn.addEventListener('click', handler);
        return btn;
    }

    function openBookmark(url, newTab) {
        if (!url)
            return;
        if (newTab && typeof OnRequestNewTab === 'function') {
            OnRequestNewTab(url);
            return;
        }
        if (typeof OnRequestChangeURL === 'function') {
            OnRequestChangeURL(url);
        } else {
            window.open(url, '_blank');
        }
    }

    function deleteNode(id) {
        if (!id || typeof OnBookmarkDelete !== 'function')
            return;
        OnBookmarkDelete(id);
        setTimeout(refreshAll, 120);
    }

    function startBookmarkCreate(parentId) {
        resetBookmarkForm();
        const folderSelect = document.getElementById('create-folder');
        if (folderSelect) {
            folderSelect.value = String(parentId || folderSelect.value || '');
        }
        const title = document.getElementById('create-title');
        if (title) title.focus();
    }

    function startBookmarkEdit(node) {
        const form = document.getElementById('bookmark-create-form');
        const title = document.getElementById('create-title');
        const url = document.getElementById('create-url');
        const folder = document.getElementById('create-folder');
        if (!form || !title || !url || !folder)
            return;
        form.dataset.mode = 'edit';
        form.dataset.editId = node.id;
        form.dataset.parentId = node.parentId || 0;
        title.value = node.title || '';
        url.value = node.url || '';
        folder.value = String(node.parentId || folder.value || '');
        updateBookmarkFormState();
        title.focus();
    }

    function startFolderCreate(parentId) {
        resetFolderForm();
        const parent = document.getElementById('folder-parent');
        if (parent)
            parent.value = String(parentId || parent.value || '');
        const name = document.getElementById('folder-name');
        if (name) name.focus();
    }

    function startFolderEdit(node) {
        const form = document.getElementById('folder-create-form');
        const name = document.getElementById('folder-name');
        const parent = document.getElementById('folder-parent');
        if (!form || !name || !parent)
            return;
        form.dataset.mode = 'edit';
        form.dataset.editId = node.id;
        form.dataset.parentId = node.parentId || 0;
        name.value = node.title || '';
        parent.value = String(node.parentId || parent.value || '');
        updateFolderFormState();
        name.focus();
    }

    function resetBookmarkForm() {
        const form = document.getElementById('bookmark-create-form');
        if (!form) return;
        delete form.dataset.mode;
        delete form.dataset.editId;
        delete form.dataset.parentId;
        updateBookmarkFormState();
    }

    function updateBookmarkFormState() {
        const form = document.getElementById('bookmark-create-form');
        const submit = document.getElementById('bookmark-submit');
        const cancel = document.getElementById('bookmark-cancel-edit');
        if (!form || !submit || !cancel) return;
        const editing = form.dataset.mode === 'edit';
        submit.textContent = editing ? 'Save changes' : 'Add bookmark';
        cancel.style.display = editing ? 'inline-flex' : 'none';
    }

    function resetFolderForm() {
        const form = document.getElementById('folder-create-form');
        if (!form) return;
        delete form.dataset.mode;
        delete form.dataset.editId;
        delete form.dataset.parentId;
        updateFolderFormState();
    }

    function updateFolderFormState() {
        const form = document.getElementById('folder-create-form');
        const submit = document.getElementById('folder-submit');
        const cancel = document.getElementById('folder-cancel-edit');
        if (!form || !submit || !cancel) return;
        const editing = form.dataset.mode === 'edit';
        submit.textContent = editing ? 'Save folder' : 'Add folder';
        cancel.style.display = editing ? 'inline-flex' : 'none';
    }

    function bindEvents() {
        const bookmarkForm = document.getElementById('bookmark-create-form');
        if (bookmarkForm)
            bookmarkForm.addEventListener('submit', onBookmarkFormSubmit);
        const folderForm = document.getElementById('folder-create-form');
        if (folderForm)
            folderForm.addEventListener('submit', onFolderFormSubmit);
        const bookmarkCancel = document.getElementById('bookmark-cancel-edit');
        if (bookmarkCancel)
            bookmarkCancel.addEventListener('click', (e) => { e.preventDefault(); resetBookmarkForm(); bookmarkForm && bookmarkForm.reset(); });
        const folderCancel = document.getElementById('folder-cancel-edit');
        if (folderCancel)
            folderCancel.addEventListener('click', (e) => { e.preventDefault(); resetFolderForm(); folderForm && folderForm.reset(); });
        const search = document.getElementById('bookmark-search');
        if (search)
            search.addEventListener('input', (e) => { state.filter = e.target.value || ''; renderTable(); });
        const refreshBtn = document.getElementById('bookmark-refresh');
        if (refreshBtn)
            refreshBtn.addEventListener('click', refreshAll);
    }

    function onBookmarkFormSubmit(e) {
        e.preventDefault();
        const form = e.currentTarget;
        const title = document.getElementById('create-title');
        const url = document.getElementById('create-url');
        const folder = document.getElementById('create-folder');
        if (!title || !url || !folder)
            return;
        const parentId = parseInt(folder.value || '0', 10) || 0;
        if (form.dataset.mode === 'edit' && form.dataset.editId) {
            if (typeof OnBookmarkUpdate === 'function')
                OnBookmarkUpdate(parseInt(form.dataset.editId, 10), title.value.trim(), url.value.trim());
            if (typeof OnBookmarkMove === 'function' && parentId !== (parseInt(form.dataset.parentId || '0', 10) || 0))
                OnBookmarkMove(parseInt(form.dataset.editId, 10), parentId, Number.MAX_SAFE_INTEGER);
        } else if (typeof OnBookmarkCreate === 'function') {
            OnBookmarkCreate(title.value.trim(), url.value.trim(), parentId);
        }
        form.reset();
        resetBookmarkForm();
        setTimeout(refreshAll, 120);
    }

    function onFolderFormSubmit(e) {
        e.preventDefault();
        const form = e.currentTarget;
        const name = document.getElementById('folder-name');
        const parent = document.getElementById('folder-parent');
        if (!name || !parent)
            return;
        const parentId = parseInt(parent.value || '0', 10) || 0;
        if (form.dataset.mode === 'edit' && form.dataset.editId) {
            if (typeof OnBookmarkUpdateFolder === 'function')
                OnBookmarkUpdateFolder(parseInt(form.dataset.editId, 10), name.value.trim());
            if (typeof OnBookmarkMove === 'function' && parentId !== (parseInt(form.dataset.parentId || '0', 10) || 0))
                OnBookmarkMove(parseInt(form.dataset.editId, 10), parentId, Number.MAX_SAFE_INTEGER);
        } else if (typeof OnBookmarkCreateFolder === 'function') {
            OnBookmarkCreateFolder(name.value.trim(), parentId);
        }
        form.reset();
        resetFolderForm();
        setTimeout(refreshAll, 120);
    }

    function renderTable() {
        const body = document.getElementById('bookmark-table-body');
        const counter = document.getElementById('results-count');
        if (!body)
            return;
        const query = state.filter.trim().toLowerCase();
        const rows = !query ? state.flat : state.flat.filter(item => {
            return item.title.toLowerCase().includes(query) || item.url.toLowerCase().includes(query) || item.path.toLowerCase().includes(query);
        });
        body.innerHTML = '';
        if (!rows.length) {
            const tr = document.createElement('tr');
            const td = document.createElement('td');
            td.colSpan = 5;
            td.className = 'empty-state';
            td.textContent = 'No bookmarks match the current filters.';
            tr.appendChild(td);
            body.appendChild(tr);
            if (counter)
                counter.textContent = '0 results';
            return;
        }
        rows.forEach(row => {
            const tr = document.createElement('tr');
            const title = document.createElement('td');
            title.textContent = row.title;
            const url = document.createElement('td');
            url.textContent = row.url;
            const path = document.createElement('td');
            path.textContent = row.path;
            const added = document.createElement('td');
            added.textContent = row.dateAdded ? new Date(row.dateAdded).toLocaleString() : '—';
            const actions = document.createElement('td');
            actions.className = 'table-actions';
            actions.appendChild(makeActionButton('Open', () => openBookmark(row.url, false)));
            actions.appendChild(makeActionButton('Edit', () => startBookmarkEdit(row)));
            actions.appendChild(makeActionButton('Delete', () => deleteNode(row.id)));
            tr.appendChild(title);
            tr.appendChild(url);
            tr.appendChild(path);
            tr.appendChild(added);
            tr.appendChild(actions);
            body.appendChild(tr);
        });
        if (counter)
            counter.textContent = rows.length + (rows.length === 1 ? ' result' : ' results');
    }

    document.addEventListener('DOMContentLoaded', init);
})();
