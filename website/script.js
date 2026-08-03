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

  // ---- FAQ accordion ----
  const faqRoot = document.querySelector("[data-faq]");
  if (faqRoot) {
    faqRoot.querySelectorAll(".faq-item").forEach((item) => {
      const btn = item.querySelector(".faq-item__btn");
      const panel = item.querySelector(".faq-item__panel");
      if (!btn || !panel) return;

      btn.addEventListener("click", () => {
        const open = btn.getAttribute("aria-expanded") === "true";

        // Close other items (single-open accordion)
        faqRoot.querySelectorAll(".faq-item").forEach((other) => {
          const oBtn = other.querySelector(".faq-item__btn");
          const oPanel = other.querySelector(".faq-item__panel");
          if (!oBtn || !oPanel || other === item) return;
          oBtn.setAttribute("aria-expanded", "false");
          oPanel.hidden = true;
        });

        btn.setAttribute("aria-expanded", String(!open));
        panel.hidden = open;
      });
    });
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
      slide.classList.toggle("is-active", n === i);
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
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;
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
