/**
 * NXstationB landing — same interactions as NXStation, separate theme key.
 */
(() => {
  "use strict";

  const THEME_KEY = "nxb-theme";
  const THEME_COLORS = { dark: "#081C15", light: "#F1FAEE" };

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
    try { localStorage.setItem(THEME_KEY, theme); } catch (_) {}
  }

  const themeToggle = document.getElementById("theme-toggle");
  if (themeToggle) {
    themeToggle.addEventListener("click", () => {
      const current = document.documentElement.getAttribute("data-theme") || "dark";
      applyTheme(current === "dark" ? "light" : "dark");
    });
    applyTheme(document.documentElement.getAttribute("data-theme") || "dark");
  }

  document.querySelectorAll('a[href="#top"]').forEach((link) => {
    link.addEventListener("click", (e) => {
      e.preventDefault();
      window.scrollTo({ top: 0, behavior: "smooth" });
      history.replaceState(null, "", "#top");
    });
  });

  const yearEl = document.getElementById("year");
  if (yearEl) yearEl.textContent = String(new Date().getFullYear());

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
    menu.querySelectorAll("a").forEach((link) => link.addEventListener("click", closeMenu));
    document.addEventListener("keydown", (e) => { if (e.key === "Escape") closeMenu(); });
  }

  const revealEls = document.querySelectorAll(".reveal");
  if ("IntersectionObserver" in window && revealEls.length) {
    const io = new IntersectionObserver((entries) => {
      entries.forEach((entry) => {
        if (entry.isIntersecting) {
          entry.target.classList.add("is-visible");
          io.unobserve(entry.target);
        }
      });
    }, { threshold: 0.12 });
    revealEls.forEach((el) => io.observe(el));
  } else {
    revealEls.forEach((el) => el.classList.add("is-visible"));
  }

  document.querySelectorAll("[data-faq] .faq-item__btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      const item = btn.closest(".faq-item");
      const panel = item.querySelector(".faq-item__panel");
      const open = btn.getAttribute("aria-expanded") === "true";
      btn.setAttribute("aria-expanded", String(!open));
      if (panel) panel.hidden = open;
    });
  });

  const carouselRoot = document.querySelector("[data-carousel]");
  if (carouselRoot) {
    const track = carouselRoot.querySelector(".carousel__track");
    const slides = [...carouselRoot.querySelectorAll("[data-slide]")];
    const prevBtn = carouselRoot.querySelector("[data-carousel-prev]");
    const nextBtn = carouselRoot.querySelector("[data-carousel-next]");
    const dotsWrap = carouselRoot.querySelector("[data-carousel-dots]");
    if (track && slides.length) {
      let index = 0;
      let autoTimer = null;
      const AUTO_MS = 5500;
      const motionOk = !window.matchMedia("(prefers-reduced-motion: reduce)").matches;

      function isGif(img) {
        if (!img) return false;
        const src = (img.dataset.gifSrc || img.currentSrc || img.src || "").toLowerCase();
        return src.endsWith(".gif") || img.dataset.animated === "true";
      }

      function pauseGif(img) {
        if (!isGif(img)) return;
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
        } catch (_) {}
      }

      function playGif(img) {
        if (isGif(img) && img.dataset.gifSrc) img.src = img.dataset.gifSrc;
      }

      function syncGif(slide, active) {
        const img = slide.querySelector(".carousel__media img");
        if (!img) return;
        if (active && motionOk) playGif(img);
        else pauseGif(img);
      }

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
          syncGif(slide, on);
        });
        dots.forEach((dot, n) => {
          dot.setAttribute("aria-selected", n === i ? "true" : "false");
        });
      }

      function goTo(i) {
        const next = (i + slides.length) % slides.length;
        const slide = slides[next];
        const target = slide.offsetLeft - (track.clientWidth - slide.offsetWidth) / 2;
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
          const dist = Math.abs(r.left + r.width / 2 - center);
          if (dist < bestDist) {
            bestDist = dist;
            best = i;
          }
        });
        return best;
      }

      let scrollTick = null;
      track.addEventListener("scroll", () => {
        if (scrollTick) return;
        scrollTick = requestAnimationFrame(() => {
          updateActive(nearestIndex());
          scrollTick = null;
        });
      }, { passive: true });

      prevBtn?.addEventListener("click", () => goTo(index - 1));
      nextBtn?.addEventListener("click", () => goTo(index + 1));
      track.addEventListener("keydown", (e) => {
        if (e.key === "ArrowLeft") { e.preventDefault(); goTo(index - 1); }
        else if (e.key === "ArrowRight") { e.preventDefault(); goTo(index + 1); }
      });

      function restartAuto() {
        if (autoTimer) clearInterval(autoTimer);
        if (!motionOk) return;
        autoTimer = setInterval(() => goTo(index + 1), AUTO_MS);
      }

      carouselRoot.addEventListener("mouseenter", () => { if (autoTimer) clearInterval(autoTimer); });
      carouselRoot.addEventListener("mouseleave", restartAuto);
      updateActive(0);
      restartAuto();
    }
  }
})();
