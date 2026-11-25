// Follow the system theme until a visitor makes a choice. Keep the preference
// in this site's browser storage; blocked storage still permits theme switching.
class PageTheme {
  constructor(stylesheet) {
    this.stylesheet = stylesheet;
    this.system = window.matchMedia('(prefers-color-scheme: dark)');
    this.choice = null;
    this.button = null;
    try {
      const saved = localStorage.getItem('pgadmin-jr-theme');
      if (saved === 'light' || saved === 'dark') this.choice = saved;
    } catch {
      // Browser privacy settings may disable storage.
    }
    this.apply();
    this.system.addEventListener('change', () => this.apply());
    document.addEventListener('DOMContentLoaded', () => this.attach());
  }

  // Apply the saved choice or current system preference before the body paints.
  apply() {
    const dark = this.choice === 'dark' || (!this.choice && this.system.matches);
    this.stylesheet.media = dark ? 'all' : 'not all';
    document.documentElement.dataset.theme = dark ? 'dark' : 'light';
    if (this.button) this.button.setAttribute('aria-pressed', String(dark));
  }

  // Reveal the working control only when JavaScript is available.
  attach() {
    this.button = document.getElementById('theme-toggle');
    this.button.hidden = false;
    this.button.addEventListener('click', () => this.toggle());
    this.apply();
  }

  // Persist an explicit toggle without making storage a requirement.
  toggle() {
    this.choice = document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark';
    try {
      localStorage.setItem('pgadmin-jr-theme', this.choice);
    } catch {
      // The current page can still switch themes.
    }
    this.apply();
  }
}

new PageTheme(document.getElementById('dark-theme'));
