window.__ul_settings = window.__ul_settings || {};

function updateBack(enable) {
	if (enable)
		document.getElementById("back").classList.remove("disabled");
	else
		document.getElementById("back").classList.add("disabled");
}

function updateForward(enable) {
	if (enable)
		document.getElementById("forward").classList.remove("disabled");
	else
		document.getElementById("forward").classList.add("disabled");
}

function updateLoading(is_loading) {
	if (is_loading) {
		document.getElementById("refresh").style.display = "none";
		document.getElementById("stop").style.display = "inline-block";
	} else {
		document.getElementById("refresh").style.display = "inline-block";
		document.getElementById("stop").style.display = "none";
	}
}

function updateURL(url) {
	document.getElementById('address').value = url;
}

function setTabDrmState(tabId, isDrm) {
	const tab = document.querySelector(".chrome-tab[data-tab-id='" + tabId + "']");
	if (!tab) return;
	const isBadgeVisible = !!isDrm;
	tab.classList.toggle('is-drm-tab', isBadgeVisible);
	const badge = tab.querySelector('.chrome-tab-badge');
	if (!badge) return;
	if (isBadgeVisible) {
		badge.textContent = '[DRM]';
		badge.title = 'This tab is using the secure DRM WebView';
	} else {
		badge.textContent = '';
		badge.removeAttribute('title');
	}
}

function focusAddressBar() {
	let address = document.getElementById('address');
	address.focus();
	address.select();
}

document.getElementById('address').addEventListener('blur', () => {
	if (window.OnAddressBarBlur) {
		window.OnAddressBarBlur();
	}
});

// Notify native when the address bar gains focus (eg, via mouse click)
document.getElementById('address').addEventListener('focus', () => {
	if (window.OnAddressBarFocus) {
		window.OnAddressBarFocus();
	}
});

// Update AdBlock toggle visual state: when enabled, normal; when disabled, grey out
function updateAdblockEnabled(enabled) {
	const el = document.getElementById('toggle-adblock');
	if (!el) return;
	const isEnabled = !!enabled;
	el.classList.remove('inactive');
	el.classList.toggle('active', isEnabled);
	el.setAttribute('aria-pressed', isEnabled ? 'true' : 'false');
	el.dataset.state = isEnabled ? 'on' : 'off';
}

function applySettings(payload) {
	let parsed = null;
	try {
		parsed = (typeof payload === 'string') ? JSON.parse(payload || '{}') : payload;
	} catch (e) {
		return;
	}
	if (!parsed || typeof parsed !== 'object') return;
	window.__ul_settings = parsed;
	const applyToBody = () => {
		const body = document.body;
		if (!body) return;
		body.classList.toggle('transparent-toolbar', !!parsed.experimental_transparent_toolbar);
		body.classList.toggle('compact-tabs', !!parsed.experimental_compact_tabs);
	};
	if (document.readyState === 'loading' && !document.body) {
		document.addEventListener('DOMContentLoaded', applyToBody, { once: true });
	} else {
		applyToBody();
	}
	if (typeof window.__ul_update_downloads_badge === 'function') {
		window.__ul_update_downloads_badge();
	}
	if (parsed.enable_suggestions === false && typeof CloseSuggestionsOverlay === 'function') {
		CloseSuggestionsOverlay();
	}
}

const bookmarkUIState = {
	visible: false,
	tree: null,
	foldersCache: [],
	contextCloser: null,
	flyoutCloser: null,
	dialog: {
		mode: 'create',
		type: 'bookmark',
		bookmarkId: 0,
		parentId: 0,
		originalParentId: 0
	}
};

function normalizeBookmarkPayload(payload) {
	if (!payload)
		return null;
	if (typeof payload === 'object')
		return payload;
	try {
		return JSON.parse(payload);
	} catch (e) {
		return null;
	}
}

function applyBookmarksBar(payload) {
	const data = normalizeBookmarkPayload(payload) || {};
	bookmarkUIState.visible = !!data.visible;
	bookmarkUIState.tree = data.tree || null;
	renderBookmarkBar();
}

function setBookmarkStarState(active) {
	const btn = document.getElementById('bookmark-star');
	if (!btn) return;
	const flag = !!active;
	btn.classList.toggle('active', flag);
	btn.setAttribute('aria-pressed', flag ? 'true' : 'false');
}

function renderBookmarkBar() {
	const bar = document.getElementById('bookmarks-bar');
	const host = document.getElementById('bookmark-bar-items');
	const collapsed = document.getElementById('bookmark-bar-collapsed');
	if (!bar || !host) return;
	const nodes = (bookmarkUIState.tree && Array.isArray(bookmarkUIState.tree.children)) ? bookmarkUIState.tree.children : [];
	host.innerHTML = '';
	if (!bookmarkUIState.visible) {
		bar.classList.remove('visible');
		bar.setAttribute('aria-hidden', 'true');
		if (collapsed) {
			collapsed.classList.add('visible');
			collapsed.setAttribute('aria-hidden', 'false');
		}
		return;
	}
	bar.classList.add('visible');
	bar.setAttribute('aria-hidden', 'false');
	if (collapsed) {
		collapsed.classList.remove('visible');
		collapsed.setAttribute('aria-hidden', 'true');
	}
	if (!nodes.length) {
		const empty = document.createElement('div');
		empty.className = 'bookmark-placeholder';
		empty.textContent = 'Bookmarks you add will appear here';
		host.appendChild(empty);
		return;
	}
	nodes.forEach(node => host.appendChild(createBookmarkChip(node)));
}

function createBookmarkChip(node) {
	const btn = document.createElement('button');
	btn.type = 'button';
	btn.className = 'bookmark-chip' + (node.type === 'folder' ? ' folder' : '');
	btn.dataset.id = node.id;
	btn.dataset.type = node.type;
	btn.title = node.type === 'folder' ? (node.title || 'Folder') : ((node.title && node.title.trim()) ? node.title : (node.url || 'Bookmark'));
	const dot = document.createElement('span');
	dot.className = 'bookmark-dot';
	btn.appendChild(dot);
	const label = document.createElement('span');
	label.className = 'bookmark-label';
	label.textContent = (node.title && node.title.trim()) ? node.title : (node.url || 'Untitled');
	btn.appendChild(label);
	btn.addEventListener('click', (evt) => {
		if (node.type === 'folder') {
			evt.preventDefault();
			openFolderFlyout(node, btn.getBoundingClientRect());
			return;
		}
		evt.preventDefault();
		const openInNewTab = evt.metaKey || evt.ctrlKey || evt.shiftKey;
		openBookmarkUrl(node.url, openInNewTab);
	});
	btn.addEventListener('auxclick', (evt) => {
		if (evt.button !== 1 || node.type === 'folder') return;
		evt.preventDefault();
		openBookmarkUrl(node.url, true);
	});
	btn.addEventListener('contextmenu', (evt) => {
		evt.preventDefault();
		openBookmarkContextMenu(node, evt.clientX, evt.clientY);
	});
	return btn;
}

function openBookmarkUrl(url, openInNewTab) {
	if (!url) return;
	if (openInNewTab && typeof OnRequestNewTab === 'function') {
		OnRequestNewTab(url);
		return;
	}
	if (typeof OnRequestChangeURL === 'function') {
		OnRequestChangeURL(url);
	}
}

function openFolderFlyout(node, rect) {
	const flyout = document.getElementById('bookmark-folder-flyout');
	if (!flyout) return;
	flyout.innerHTML = '';
	const children = Array.isArray(node.children) ? node.children : [];
	if (!children.length) {
		const empty = document.createElement('div');
		empty.className = 'folder-entry';
		empty.textContent = 'Empty folder';
		flyout.appendChild(empty);
	} else {
		const container = document.createElement('div');
		container.className = 'folder-entry';
		const childList = document.createElement('div');
		childList.className = 'folder-children';
		children.forEach((child) => {
			const btn = document.createElement('button');
			btn.type = 'button';
			btn.className = 'folder-child';
			btn.textContent = (child.title && child.title.trim()) ? child.title : (child.url || 'Untitled');
			btn.addEventListener('click', (evt) => {
				evt.preventDefault();
				if (child.type === 'folder') {
					openFolderFlyout(child, btn.getBoundingClientRect());
				} else {
					openBookmarkUrl(child.url, evt.metaKey || evt.ctrlKey || evt.shiftKey);
					closeFolderFlyout();
				}
			});
			btn.addEventListener('contextmenu', (evt) => {
				evt.preventDefault();
				openBookmarkContextMenu(child, evt.clientX, evt.clientY);
			});
			childList.appendChild(btn);
		});
		container.appendChild(childList);
		flyout.appendChild(container);
	}
	flyout.classList.remove('hidden');
	flyout.setAttribute('aria-hidden', 'false');
	const spacing = 4;
	let left = rect.left;
	let top = rect.bottom + spacing;
	const width = flyout.offsetWidth || 220;
	const height = flyout.offsetHeight || 240;
	if (left + width > window.innerWidth)
		left = window.innerWidth - width - spacing;
	if (top + height > window.innerHeight)
		top = rect.top - height - spacing;
	flyout.style.left = Math.max(8, Math.round(left)) + 'px';
	flyout.style.top = Math.max(8, Math.round(top)) + 'px';
	if (bookmarkUIState.flyoutCloser)
		document.removeEventListener('mousedown', bookmarkUIState.flyoutCloser);
	bookmarkUIState.flyoutCloser = (evt) => {
		if (!flyout.contains(evt.target))
			closeFolderFlyout();
	};
	setTimeout(() => document.addEventListener('mousedown', bookmarkUIState.flyoutCloser), 0);
}

function closeFolderFlyout() {
	const flyout = document.getElementById('bookmark-folder-flyout');
	if (!flyout) return;
	flyout.classList.add('hidden');
	flyout.setAttribute('aria-hidden', 'true');
	if (bookmarkUIState.flyoutCloser) {
		document.removeEventListener('mousedown', bookmarkUIState.flyoutCloser);
		bookmarkUIState.flyoutCloser = null;
	}
}

function openBookmarkContextMenu(node, x, y) {
	const actions = [];
	const separator = () => actions.push({ separator: true });
	if (node.type === 'folder') {
		actions.push({ label: 'Open all in new tabs', handler: () => openAllBookmarks(node) });
		actions.push({ label: 'Add page to folder', handler: () => openBookmarkDialog({ mode: 'create', type: 'bookmark', parentId: node.id }) });
		actions.push({ label: 'Add subfolder', handler: () => openBookmarkDialog({ mode: 'create', type: 'folder', parentId: node.id }) });
		separator();
		actions.push({ label: 'Rename folder', handler: () => openBookmarkDialog({ mode: 'edit', type: 'folder', title: node.title || '', bookmarkId: node.id, parentId: node.parentId || (bookmarkUIState.tree ? bookmarkUIState.tree.id : 0) }) });
		actions.push({ label: 'Delete folder', handler: () => deleteBookmark(node.id), danger: true });
	}
	else {
		actions.push({ label: 'Open', handler: () => openBookmarkUrl(node.url, false) });
		actions.push({ label: 'Open in new tab', handler: () => openBookmarkUrl(node.url, true) });
		separator();
		actions.push({ label: 'Edit...', handler: () => openBookmarkDialog({ mode: 'edit', type: 'bookmark', title: node.title || '', url: node.url || '', bookmarkId: node.id, parentId: node.parentId || (bookmarkUIState.tree ? bookmarkUIState.tree.id : 0) }) });
		actions.push({ label: 'Delete', handler: () => deleteBookmark(node.id), danger: true });
	}
	showBookmarkMenu(actions, x, y);
}

function openBookmarkUtilityMenu(anchor) {
	const rect = anchor.getBoundingClientRect();
	const x = rect.left;
	const y = rect.bottom + 6;
	const actions = [
		{ label: 'Add bookmark...', handler: () => openBookmarkDialog({ mode: 'create', type: 'bookmark', parentId: bookmarkUIState.tree ? bookmarkUIState.tree.id : 0 }) },
		{ label: 'Add folder...', handler: () => openBookmarkDialog({ mode: 'create', type: 'folder', parentId: bookmarkUIState.tree ? bookmarkUIState.tree.id : 0 }) },
		{ label: 'Bookmark manager', handler: () => { if (typeof OnShowBookmarkManager === 'function') OnShowBookmarkManager(); } },
		{ separator: true },
		{ label: bookmarkUIState.visible ? 'Hide bookmarks bar' : 'Show bookmarks bar', handler: () => { if (typeof OnToggleBookmarkBarCommand === 'function') OnToggleBookmarkBarCommand(); } }
	];
	showBookmarkMenu(actions, x, y);
}

function showBookmarkMenu(actions, x, y) {
	const menu = document.getElementById('bookmark-context-menu');
	if (!menu || !actions || !actions.length) return;
	menu.innerHTML = '';
	actions.forEach(action => {
		if (action.separator) {
			const sep = document.createElement('div');
			sep.className = 'menu-separator';
			menu.appendChild(sep);
			return;
		}
		const btn = document.createElement('button');
		btn.type = 'button';
		btn.textContent = action.label;
		if (action.danger)
			btn.classList.add('danger');
		btn.addEventListener('click', () => {
			hideBookmarkMenu();
			try {
				action.handler();
			} catch (e) { }
		});
		menu.appendChild(btn);
	});
	menu.classList.remove('hidden');
	menu.setAttribute('aria-hidden', 'false');
	const width = menu.offsetWidth || 200;
	const height = menu.offsetHeight || (actions.length * 32);
	let left = x;
	let top = y;
	if (left + width > window.innerWidth)
		left = window.innerWidth - width - 8;
	if (top + height > window.innerHeight)
		top = window.innerHeight - height - 8;
	menu.style.left = Math.max(8, Math.round(left)) + 'px';
	menu.style.top = Math.max(8, Math.round(top)) + 'px';
	if (bookmarkUIState.contextCloser)
		document.removeEventListener('mousedown', bookmarkUIState.contextCloser);
	bookmarkUIState.contextCloser = (evt) => {
		if (!menu.contains(evt.target))
			hideBookmarkMenu();
	};
	setTimeout(() => document.addEventListener('mousedown', bookmarkUIState.contextCloser), 0);
}

function hideBookmarkMenu() {
	const menu = document.getElementById('bookmark-context-menu');
	if (!menu) return;
	menu.classList.add('hidden');
	menu.setAttribute('aria-hidden', 'true');
	if (bookmarkUIState.contextCloser) {
		document.removeEventListener('mousedown', bookmarkUIState.contextCloser);
		bookmarkUIState.contextCloser = null;
	}
}

function openAllBookmarks(node) {
	if (!node || !Array.isArray(node.children)) return;
	node.children.forEach(child => {
		if (child.type === 'bookmark')
			openBookmarkUrl(child.url, true);
	});
}

function deleteBookmark(id) {
	if (!id || typeof OnBookmarkDelete !== 'function')
		return;
	OnBookmarkDelete(id);
}

function openBookmarkDialog(payload) {
	const data = normalizeBookmarkPayload(payload) || {};
	const backdrop = document.getElementById('bookmark-dialog-backdrop');
	if (!backdrop) return;
	const type = data.type === 'folder' ? 'folder' : 'bookmark';
	const mode = data.mode === 'edit' ? 'edit' : 'create';
	const defaultParent = data.parentId || (bookmarkUIState.tree ? bookmarkUIState.tree.id : 0) || 0;
	bookmarkUIState.dialog = {
		mode,
		type,
		bookmarkId: data.bookmarkId || 0,
		parentId: defaultParent,
		originalParentId: defaultParent
	};
	const titleInput = document.getElementById('bookmark-dialog-title');
	const urlInput = document.getElementById('bookmark-dialog-url');
	const urlField = document.getElementById('bookmark-url-field');
	if (titleInput)
		titleInput.value = data.title || '';
	if (urlInput) {
		urlInput.value = data.url || '';
		urlInput.disabled = (type === 'folder');
	}
	if (urlField)
		urlField.style.display = (type === 'folder') ? 'none' : 'flex';
	populateBookmarkFolderSelect(defaultParent, true);
	const eyebrow = document.getElementById('bookmark-dialog-mode');
	if (eyebrow)
		eyebrow.textContent = (mode === 'edit' ? 'Edit ' : 'Add ') + (type === 'folder' ? 'folder' : 'bookmark');
	backdrop.classList.remove('hidden');
	backdrop.setAttribute('aria-hidden', 'false');
	document.body.classList.add('bookmark-dialog-open');
	setTimeout(() => {
		if (titleInput)
			titleInput.focus();
	}, 0);
}

function closeBookmarkDialog() {
	const backdrop = document.getElementById('bookmark-dialog-backdrop');
	if (!backdrop) return;
	backdrop.classList.add('hidden');
	backdrop.setAttribute('aria-hidden', 'true');
	document.body.classList.remove('bookmark-dialog-open');
}

function invokeBookmarkToggleFallback() {
	if (typeof OnToggleBookmarkForActiveTab === 'function') {
		OnToggleBookmarkForActiveTab();
		return true;
	}
	if (typeof GetActiveBookmarkInfo !== 'function')
		return false;
	try {
		const raw = GetActiveBookmarkInfo();
		if (!raw)
			return false;
		const info = typeof raw === 'string' ? JSON.parse(raw) : raw;
		if (!info)
			return false;
		openBookmarkDialog({
			mode: info.isBookmarked ? 'edit' : 'create',
			type: 'bookmark',
			title: info.title || '',
			url: info.url || '',
			bookmarkId: info.bookmarkId || 0,
			parentId: info.parentId || (bookmarkUIState.tree ? bookmarkUIState.tree.id : 0)
		});
		return true;
	} catch (err) {
		console.error('Failed to open bookmark dialog fallback', err);
	}
	return false;
}

function submitBookmarkDialog(evt) {
	if (evt) evt.preventDefault();
	const titleInput = document.getElementById('bookmark-dialog-title');
	const urlInput = document.getElementById('bookmark-dialog-url');
	const folderSelect = document.getElementById('bookmark-dialog-folder');
	if (!titleInput || !folderSelect)
		return;
	const title = titleInput.value.trim();
	const url = urlInput && !urlInput.disabled ? urlInput.value.trim() : '';
	const parentId = parseInt(folderSelect.value || '0', 10) || 0;
	const dialogState = bookmarkUIState.dialog;
	if (dialogState.type === 'bookmark' && !url) {
		if (urlInput) urlInput.focus();
		return;
	}
	if (dialogState.type === 'bookmark') {
		if (dialogState.mode === 'edit' && dialogState.bookmarkId && typeof OnBookmarkUpdate === 'function') {
			OnBookmarkUpdate(dialogState.bookmarkId, title, url);
		} else if (typeof OnBookmarkCreate === 'function') {
			OnBookmarkCreate(title, url, parentId);
		}
	} else {
		if (dialogState.mode === 'edit' && dialogState.bookmarkId && typeof OnBookmarkUpdateFolder === 'function') {
			OnBookmarkUpdateFolder(dialogState.bookmarkId, title);
		} else if (typeof OnBookmarkCreateFolder === 'function') {
			OnBookmarkCreateFolder(title, parentId);
		}
	}
	if (dialogState.mode === 'edit' && dialogState.bookmarkId && parentId !== dialogState.originalParentId && typeof OnBookmarkMove === 'function') {
		OnBookmarkMove(dialogState.bookmarkId, parentId, Number.MAX_SAFE_INTEGER);
	}
	closeBookmarkDialog();
}

function populateBookmarkFolderSelect(selectedId, forceRefresh) {
	const select = document.getElementById('bookmark-dialog-folder');
	if (!select || typeof GetBookmarkFolders !== 'function')
		return;
	if (forceRefresh)
		bookmarkUIState.foldersCache = [];
	if (!bookmarkUIState.foldersCache.length) {
		try {
			const raw = GetBookmarkFolders();
			bookmarkUIState.foldersCache = JSON.parse(raw || '[]');
		} catch (e) {
			bookmarkUIState.foldersCache = [];
		}
	}
	select.innerHTML = '';
	bookmarkUIState.foldersCache.forEach(folder => {
		const option = document.createElement('option');
		option.value = folder.id;
		option.textContent = folder.path || folder.title || 'Folder';
		select.appendChild(option);
	});
	if (selectedId)
		select.value = String(selectedId);
}

function attachBookmarkEventHandlers() {
	const starBtn = document.getElementById('bookmark-star');
	if (starBtn) {
		const toggle = () => {
			if (!invokeBookmarkToggleFallback())
				console.warn('Bookmark toggle bridge is unavailable.');
		};
		['click'].forEach(evt => starBtn.addEventListener(evt, (e) => { e.preventDefault(); toggle(); }));
		starBtn.addEventListener('keydown', (e) => {
			if (e.key === 'Enter' || e.key === ' ') {
				e.preventDefault();
				toggle();
			}
		});
	}
	const menuBtn = document.getElementById('bookmark-bar-menu');
	if (menuBtn) {
		menuBtn.addEventListener('click', (e) => {
			e.preventDefault();
			openBookmarkUtilityMenu(menuBtn);
		});
	}
	const form = document.getElementById('bookmark-dialog-form');
	if (form)
		form.addEventListener('submit', submitBookmarkDialog);
	const cancelBtn = document.getElementById('bookmark-dialog-cancel');
	if (cancelBtn)
		cancelBtn.addEventListener('click', (e) => { e.preventDefault(); closeBookmarkDialog(); });
	const closeBtn = document.getElementById('bookmark-dialog-close');
	if (closeBtn)
		closeBtn.addEventListener('click', (e) => { e.preventDefault(); closeBookmarkDialog(); });
	const collapsedBtn = document.getElementById('bookmark-bar-collapsed-button');
	if (collapsedBtn) {
		collapsedBtn.addEventListener('click', (e) => {
			e.preventDefault();
			if (typeof OnToggleBookmarkBarCommand === 'function')
				OnToggleBookmarkBarCommand();
		});
	}
	window.addEventListener('keydown', (e) => {
		if (e.key === 'Escape') {
			hideBookmarkMenu();
			closeFolderFlyout();
			const dialog = document.getElementById('bookmark-dialog-backdrop');
			if (dialog && !dialog.classList.contains('hidden'))
				closeBookmarkDialog();
		}
	});
	window.addEventListener('resize', () => {
		hideBookmarkMenu();
		closeFolderFlyout();
	});
}

if (document.readyState === 'loading')
	document.addEventListener('DOMContentLoaded', attachBookmarkEventHandlers, { once: true });
else
	attachBookmarkEventHandlers();