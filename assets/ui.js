window.__ul_settings = window.__ul_settings || {};

// Listen for theme changes to update toolbar icon colors
window.addEventListener('themeChanged', function(e) {
    // Get the computed style to force CSS variable recalculation
    const computedStyle = getComputedStyle(document.documentElement);
    const newIconColor = computedStyle.getPropertyValue('--toolbar-icon-color').trim();
    const newDisabledColor = computedStyle.getPropertyValue('--toolbar-icon-disabled').trim();
    
    // Force repaint of SVG icons by explicitly setting fill from CSS variables
    const icons = document.querySelectorAll('.icon:not(.disabled)');
    icons.forEach(function(icon) {
        // Clear inline style and force recalculation
        icon.style.fill = '';
        // Force reflow
        void icon.offsetHeight;
        // Apply from CSS variable
        if (newIconColor) {
            icon.style.fill = 'var(--toolbar-icon-color)';
        }
    });
    
    // Update disabled icons too
    const disabledIcons = document.querySelectorAll('.icon.disabled');
    disabledIcons.forEach(function(icon) {
        icon.style.fill = '';
        void icon.offsetHeight;
        if (newDisabledColor) {
            icon.style.fill = 'var(--toolbar-icon-disabled)';
        }
    });
    
    // Force update of session restore bar and other themed elements
    const sessionBar = document.getElementById('session-restore-bar');
    if (sessionBar) {
        sessionBar.style.display = sessionBar.style.display;
    }
    
    // Trigger a full document repaint for any remaining elements
    document.body.style.display = 'none';
    void document.body.offsetHeight;
    document.body.style.display = '';
});

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