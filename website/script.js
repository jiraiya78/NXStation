/**
 * NXStation landing — interactions
 * Mobile nav, scroll reveals, FAQ accordion, carousel, back-to-top
 */

(() => {
  "use strict";

  const THEME_KEY = "nx-theme";
  const THEME_COLORS = { dark: "#0C0612", light: "#FAF5FF" };

  function applyTheme(theme) {
    document.documentElement.setAttribute("data-theme", theme);
    const metaTheme = document.querySelector('meta[name="theme-color"]');
    if (metaTheme) metaTheme.setAttribute("content", THEME_COLORS[theme]);
    const themeToggle = document.getElementById("theme-toggle");
    if (themeToggle) {
      const label = theme === "dark" ? "Switch to light mode" : "Switch to dark mode";
      themeToggle.setAttribute("aria-label", label);
      themeToggle.setAttribute("title", theme === "dark" ? "Light mode" : "Dark mode");
    }
    try {
      localStorage.setItem(THEME_KEY, theme);
    } catch (_) {
      /* storage unavailable */
    }
  }

  const themeToggle = document.getElementById("theme-toggle");
  if (themeToggle) {
    themeToggle.addEventListener("click", () => {
      const current = document.documentElement.getAttribute("data-theme") || "dark";
      applyTheme(current === "dark" ? "light" : "dark");
    });
    applyTheme(document.documentElement.getAttribute("data-theme") || "dark");
  }

  // ---- Brand / home link — scroll to true page top ----
  document.querySelectorAll('a[href="#top"]').forEach((link) => {
    link.addEventListener("click", (e) => {
      e.preventDefault();
      window.scrollTo({ top: 0, behavior: "smooth" });
      history.replaceState(null, "", "#top");
    });
  });

  // ---- Year in footer ----
  const yearEl = document.getElementById("year");
  if (yearEl) yearEl.textContent = String(new Date().getFullYear());

  // ---- Mobile nav ----
  const toggle = document.getElementById("nav-toggle");
  const menu = document.getElementById("nav-menu");

  if (toggle && menu) {
    const closeMenu = () => {
      toggle.setAttribute("aria-expanded", "false");
      menu.classList.remove("is-open");
    };

    toggle.addEventListener("click", () => {
      const open = toggle.getAttribute("aria-expanded") === "true";
      toggle.setAttribute("aria-expanded", String(!open));
      menu.classList.toggle("is-open", !open);
    });

    menu.querySelectorAll("a").forEach((link) => {
      link.addEventListener("click", closeMenu);
    });

    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") closeMenu();
    });
  }

  // ---- Scroll-triggered reveals ----
  const revealEls = document.querySelectorAll(".reveal");

  if ("IntersectionObserver" in window && revealEls.length) {
    const io = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target.classList.add("is-visible");
            io.unobserve(entry.target);
          }
        });
      },
      { rootMargin: "0px 0px -8% 0px", threshold: 0.12 }
    );

    revealEls.forEach((el) => io.observe(el));
  } else {
    revealEls.forEach((el) => el.classList.add("is-visible"));
  }

  // ---- Back to top ----
  const backBtn = document.getElementById("back-to-top");

  if (backBtn) {
    const onScroll = () => {
      const show = window.scrollY > 480;
      backBtn.hidden = !show;
    };

    window.addEventListener("scroll", onScroll, { passive: true });
    onScroll();

    backBtn.addEventListener("click", () => {
      window.scrollTo({ top: 0, behavior: "smooth" });
    });
  }

  // ---- FAQ / Wiki accordion ----
  document.querySelectorAll("[data-faq], [data-wiki-accordion]").forEach((faqRoot) => {
    const allowMultiple = faqRoot.hasAttribute("data-allow-multiple");

    faqRoot.querySelectorAll(".faq-item").forEach((item) => {
      const btn = item.querySelector(".faq-item__btn");
      const panel = item.querySelector(".faq-item__panel");
      if (!btn || !panel) return;

      btn.addEventListener("click", () => {
        const open = btn.getAttribute("aria-expanded") === "true";

        if (!allowMultiple) {
          faqRoot.querySelectorAll(".faq-item").forEach((other) => {
            const oBtn = other.querySelector(".faq-item__btn");
            const oPanel = other.querySelector(".faq-item__panel");
            if (!oBtn || !oPanel || other === item) return;
            oBtn.setAttribute("aria-expanded", "false");
            oPanel.hidden = true;
          });
        }

        btn.setAttribute("aria-expanded", String(!open));
        panel.hidden = open;
      });
    });
  });

  // Wiki: expand section linked from table of contents hash
  if (location.hash) {
    const section = document.querySelector(location.hash);
    if (section?.classList.contains("faq-item")) {
      const btn = section.querySelector(".faq-item__btn");
      const panel = section.querySelector(".faq-item__panel");
      if (btn && panel) {
        btn.setAttribute("aria-expanded", "true");
        panel.hidden = false;
        section.scrollIntoView({ behavior: "smooth", block: "start" });
      }
    }
  }

  // ---- UI carousel (scroll-snap + controls) ----
  const carouselRoot = document.querySelector("[data-carousel]");
  if (carouselRoot) {

  const track = carouselRoot.querySelector(".carousel__track");
  const slides = [...carouselRoot.querySelectorAll("[data-slide]")];
  const prevBtn = carouselRoot.querySelector("[data-carousel-prev]");
  const nextBtn = carouselRoot.querySelector("[data-carousel-next]");
  const dotsWrap = carouselRoot.querySelector("[data-carousel-dots]");

  if (!track || slides.length === 0) {
    /* no carousel slides */
  } else {

  let index = 0;
  let autoTimer = null;
  const AUTO_MS = 5500;
  const motionOk = !window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  function isAnimatedImage(img) {
    if (!img) return false;
    const src = (img.dataset.gifSrc || img.currentSrc || img.src || "").toLowerCase();
    return src.endsWith(".gif") || img.dataset.animated === "true";
  }

  function advanceImageFallback(img) {
    const queued = (img.dataset.fallbacks || "")
      .split(",")
      .map((s) => s.trim())
      .filter(Boolean);
    if (queued.length > 0) {
      img.src = queued[0];
      img.dataset.fallbacks = queued.slice(1).join(",");
      delete img.dataset.fallbackTried;
      return true;
    }
    if (img.dataset.fallbackTried) return false;
    img.dataset.fallbackTried = "1";
    const base = (img.dataset.gifSrc || img.getAttribute("src") || "")
      .split("/")
      .pop()
      .replace(/\.(gif|jpg|jpeg|png|webp|svg)$/i, "");
    if (base) img.src = `assets/UI-carousel/${base}.svg`;
    return false;
  }

  function ensureImageLoads(img) {
    const verify = () => {
      if (img.naturalWidth === 0 && advanceImageFallback(img)) {
        ensureImageLoads(img);
      }
    };
    const onError = () => {
      if (advanceImageFallback(img)) ensureImageLoads(img);
    };
    if (img.complete) {
      verify();
    } else {
      img.addEventListener("load", verify, { once: true });
      img.addEventListener("error", onError, { once: true });
    }
  }

  function showVideoFallback(slide) {
    const video = slide.querySelector("video.carousel__video");
    const fallback = slide.querySelector("img.carousel__fallback");
    if (!video || !fallback) return;
    video.hidden = true;
    video.pause();
    fallback.hidden = false;
    ensureImageLoads(fallback);
  }

  function pauseGif(img) {
    if (!isAnimatedImage(img)) return;
    if (!img.dataset.gifSrc) img.dataset.gifSrc = img.currentSrc || img.src;
    try {
      const canvas = document.createElement("canvas");
      const w = img.naturalWidth || img.width;
      const h = img.naturalHeight || img.height;
      if (!w || !h) return;
      canvas.width = w;
      canvas.height = h;
      canvas.getContext("2d").drawImage(img, 0, 0);
      img.src = canvas.toDataURL("image/jpeg", 0.9);
    } catch (_) {
      /* canvas taint or zero size */
    }
  }

  function playGif(img) {
    if (!isAnimatedImage(img)) return;
    if (img.dataset.gifSrc) img.src = img.dataset.gifSrc;
  }

  async function probeMediaUrl(url) {
    if (!url) return false;
    try {
      const res = await fetch(url, { method: "HEAD", cache: "no-store" });
      return res.ok;
    } catch {
      return false;
    }
  }

  function initVideoFallback(slide) {
    const video = slide.querySelector("video.carousel__video");
    const fallback = slide.querySelector("img.carousel__fallback");
    if (!video || !fallback) return;

    const showFallback = () => showVideoFallback(slide);

    video.addEventListener("error", showFallback);
    video.querySelectorAll("source").forEach((source) => {
      source.addEventListener("error", showFallback);
    });

    video.load();

    const checkVideoUnavailable = () => {
      if (video.hidden) return;
      if (video.error || video.networkState === HTMLMediaElement.NETWORK_NO_SOURCE) {
        showFallback();
      }
    };

    requestAnimationFrame(checkVideoUnavailable);
    setTimeout(checkVideoUnavailable, 400);

    probeMediaUrl(video.querySelector("source[type=\"video/mp4\"]")?.src).then((mp4Ok) => {
      if (mp4Ok) return true;
      return probeMediaUrl(video.querySelector("source[type=\"video/webm\"]")?.src);
    }).then((hasVideo) => {
      if (hasVideo === false) showFallback();
    });
  }

  function syncSlideMedia(slide, active) {
    const video = slide.querySelector("video.carousel__video");
    const fallback = slide.querySelector("img.carousel__fallback");
    const gif =
      slide.querySelector(".carousel__media img:not(.carousel__fallback):not([hidden])") ||
      (fallback && !fallback.hidden ? fallback : null);

    if (video && !video.hidden) {
      if (active && motionOk) {
        video.play().catch(() => showVideoFallback(slide));
      } else {
        video.pause();
        if (!active) video.currentTime = 0;
      }
    }

    if (gif && isAnimatedImage(gif)) {
      if (active && motionOk) playGif(gif);
      else pauseGif(gif);
    }
  }

  function initImageFallback(slide) {
    slide.querySelectorAll(".carousel__media img").forEach((img) => {
      img.addEventListener("error", () => advanceImageFallback(img));
      if (!img.hidden) ensureImageLoads(img);
    });
  }

  slides.forEach((slide) => {
    initVideoFallback(slide);
    initImageFallback(slide);
  });

  const dots = slides.map((_, i) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "carousel__dot";
    btn.setAttribute("role", "tab");
    btn.setAttribute("aria-label", `Go to slide ${i + 1}`);
    btn.setAttribute("aria-selected", i === 0 ? "true" : "false");
    btn.addEventListener("click", () => goTo(i));
    dotsWrap?.appendChild(btn);
    return btn;
  });

  function updateActive(i) {
    index = i;
    slides.forEach((slide, n) => {
      const on = n === i;
      slide.classList.toggle("is-active", on);
      syncSlideMedia(slide, on);
    });
    dots.forEach((dot, n) => {
      dot.setAttribute("aria-selected", n === i ? "true" : "false");
    });
  }

  function goTo(i) {
    const next = (i + slides.length) % slides.length;
    const slide = slides[next];
    // Scroll only the track horizontally — never scrollIntoView (it hijacks page scroll)
    const target =
      slide.offsetLeft - (track.clientWidth - slide.offsetWidth) / 2;
    track.scrollTo({ left: Math.max(0, target), behavior: "smooth" });
    updateActive(next);
    restartAuto();
  }

  function nearestIndex() {
    const trackRect = track.getBoundingClientRect();
    const center = trackRect.left + trackRect.width / 2;
    let best = 0;
    let bestDist = Infinity;

    slides.forEach((slide, i) => {
      const r = slide.getBoundingClientRect();
      const mid = r.left + r.width / 2;
      const dist = Math.abs(mid - center);
      if (dist < bestDist) {
        bestDist = dist;
        best = i;
      }
    });

    return best;
  }

  let scrollTick = null;
  track.addEventListener(
    "scroll",
    () => {
      if (scrollTick) return;
      scrollTick = requestAnimationFrame(() => {
        updateActive(nearestIndex());
        scrollTick = null;
      });
    },
    { passive: true }
  );

  prevBtn?.addEventListener("click", () => goTo(index - 1));
  nextBtn?.addEventListener("click", () => goTo(index + 1));

  track.addEventListener("keydown", (e) => {
    if (e.key === "ArrowLeft") {
      e.preventDefault();
      goTo(index - 1);
    } else if (e.key === "ArrowRight") {
      e.preventDefault();
      goTo(index + 1);
    }
  });

  function restartAuto() {
    if (autoTimer) clearInterval(autoTimer);
    if (!motionOk) return;
    autoTimer = setInterval(() => goTo(index + 1), AUTO_MS);
  }

  carouselRoot.addEventListener("mouseenter", () => {
    if (autoTimer) clearInterval(autoTimer);
  });
  carouselRoot.addEventListener("mouseleave", restartAuto);
  carouselRoot.addEventListener("focusin", () => {
    if (autoTimer) clearInterval(autoTimer);
  });
  carouselRoot.addEventListener("focusout", (e) => {
    if (!carouselRoot.contains(e.relatedTarget)) restartAuto();
  });

  updateActive(0);
  restartAuto();
  }
  }
})();
