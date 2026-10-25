(function (root, factory) {
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = factory();
    } else {
        root.Siglent = factory();
    }
}(typeof self !== 'undefined' ? self : this, function () {
    'use strict';

    function buildWsUrl(protocol, host) {
        return (protocol === 'https:' ? 'wss' : 'ws') + '://' + host + '/ws';
    }

    function isValidHexColor(color) {
        return typeof color === 'string' && /^#[0-9a-fA-F]{6}$/.test(color);
    }

    // 0-255 → whole percent
    function brightnessPercent(brightness) {
        return Math.round(brightness * 100 / 255);
    }

    // Calls fn at most once per `ms`. The first call goes straight out; calls inside the
    // window collapse into one trailing call with the latest arguments, so the final
    // value of a drag is never lost.
    function throttle(fn, ms) {
        var last    = 0;
        var timer   = null;
        var pending = null;

        function fire() {
            var args = pending;
            last    = Date.now();
            timer   = null;
            pending = null;
            fn.apply(null, args);
        }

        return function () {
            pending = arguments;
            if (timer) return;
            var wait = ms - (Date.now() - last);
            if (wait <= 0) fire();
            else timer = setTimeout(fire, wait);
        };
    }

    return {
        buildWsUrl:        buildWsUrl,
        isValidHexColor:   isValidHexColor,
        brightnessPercent: brightnessPercent,
        throttle:          throttle,
    };
}));
