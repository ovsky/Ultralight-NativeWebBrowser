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