/**
 * Cookie consent + deferred Google Analytics (loads only after acceptance).
 */
(() => {
  "use strict";

  const CONSENT_KEY = "nx-cookie-consent";
  const GA_ID = "G-N2KGV0YEWN";

  function getConsent() {
    try {
      return localStorage.getItem(CONSENT_KEY);
    } catch {
      return null;
    }
  }

  function setConsent(value) {
    try {
      localStorage.setItem(CONSENT_KEY, value);
    } catch {
      /* storage unavailable */
    }
  }

  function loadAnalytics() {
    if (window.__nxGaLoaded) return;
    window.__nxGaLoaded = true;

    window.dataLayer = window.dataLayer || [];
    function gtag() {
      window.dataLayer.push(arguments);
    }
    window.gtag = gtag;

    const script = document.createElement("script");
    script.async = true;
    script.src = `https://www.googletagmanager.com/gtag/js?id=${GA_ID}`;
    script.onload = () => {
      gtag("js", new Date());
      gtag("config", GA_ID, { anonymize_ip: true });
    };
    document.head.appendChild(script);
  }

  function hideBanner() {
    const banner = document.getElementById("cookie-consent");
    if (banner) banner.hidden = true;
    document.body.classList.remove("cookie-banner-visible");
  }

  function showBanner() {
    if (document.getElementById("cookie-consent")) return;

    const privacyHref = document.querySelector('a[href="privacy.html"]')?.getAttribute("href") || "privacy.html";

    const banner = document.createElement("div");
    banner.id = "cookie-consent";
    banner.className = "cookie-consent glass";
    banner.setAttribute("role", "dialog");
    banner.setAttribute("aria-live", "polite");
    banner.setAttribute("aria-label", "Cookie consent");

    banner.innerHTML = `
      <div class="cookie-consent__inner">
        <p class="cookie-consent__text">
          We use cookies for basic Google Analytics to understand how visitors use this site.
          The NXStation app on your Switch does not use these cookies.
          <a href="${privacyHref}">Privacy Policy</a>
        </p>
        <div class="cookie-consent__actions">
          <button type="button" class="btn btn--secondary cookie-consent__btn" data-cookie-decline>
            Decline
          </button>
          <button type="button" class="btn btn--primary cookie-consent__btn" data-cookie-accept>
            Accept
          </button>
        </div>
      </div>
    `;

    banner.querySelector("[data-cookie-accept]").addEventListener("click", () => {
      setConsent("accepted");
      hideBanner();
      loadAnalytics();
    });

    banner.querySelector("[data-cookie-decline]").addEventListener("click", () => {
      setConsent("declined");
      hideBanner();
    });

    document.body.appendChild(banner);
    document.body.classList.add("cookie-banner-visible");
  }

  function init() {
    const consent = getConsent();
    if (consent === "accepted") {
      loadAnalytics();
      return;
    }
    if (consent !== "declined") showBanner();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
