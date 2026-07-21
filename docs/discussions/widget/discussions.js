// Blog discussions widget. Fetches a page's discussion and comments from its backend
// and renders them in a threaded timeline. Comment bodies arrive as sanitized,
// server-rendered HTML, so the widget only owns the surrounding chrome.
//
// Readers sign in through the tenant's OAuth provider for identity; comments/replies/edits/reactions
// are stored by the backend. Signed-out readers see a read-only timeline and the tenant's
// sign-in button.
(function () {
  // Official GitHub mark (Octocat), inlined so the sign-in button needs no asset.
  const GITHUB_MARK_SVG =
    '<svg class="gc-gh-mark" viewBox="0 0 16 16" width="16" height="16" aria-hidden="true">' +
    '<path fill="currentColor" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 ' +
    '0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 ' +
    '1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-' +
    '.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 ' +
    '1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 ' +
    '2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"></path></svg>';

  const REACTION = {
    THUMBS_UP: "👍", THUMBS_DOWN: "👎", LAUGH: "😄", HOORAY: "🎉",
    CONFUSED: "😕", HEART: "❤️", ROCKET: "🚀", EYES: "👀",
  };
  const REACTION_ORDER = [
    "THUMBS_UP", "THUMBS_DOWN", "LAUGH", "HOORAY", "CONFUSED", "HEART", "ROCKET", "EYES",
  ];

  // GitHub's minimize reasons (ReportedContentClassifiers) + display label + emoji.
  // GitHub's minimize reasons: code, menu label, emoji (menu only), past-tense phrase
  // for the collapse-bar sentence ("This comment was <phrase> <when>.").
  const HIDE_REASONS = [
    ["OFF_TOPIC", "Off-topic", "💬", "marked as off-topic"],
    ["OUTDATED", "Outdated", "🕒", "marked as outdated"],
    ["DUPLICATE", "Duplicate", "🔁", "marked as a duplicate"],
    ["RESOLVED", "Resolved", "✅", "resolved"],
    ["SPAM", "Spam", "🥫", "marked as spam"],
    ["ABUSE", "Abuse", "🚫", "marked as abuse"],
  ];
  // Look up a hide reason's {label, emoji, phrase} by code (e.g. "OUTDATED"); null if unknown.
  function hideReason(code) {
    for (const r of HIDE_REASONS) if (r[0] === code) return { label: r[1], emoji: r[2], phrase: r[3] };
    return null;
  }

  // Shared across renders: active config, signed-in state, the widget root, and the
  // current viewer. renderComment/reply controls read these so they don't need them
  // threaded through every call; all are set in mount/refresh before (re)rendering.
  let CFG = null;
  let SIGNED_IN = false;
  let ROOT = null;
  let ME = null;

  // Octicon paths (MIT, github/primer) for the markdown toolbar.
  const OCTICONS = {
    bold: "M4 2h4.5a3.501 3.501 0 0 1 2.852 5.53A3.499 3.499 0 0 1 9.5 14H4a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1Zm1 7v3h4.5a1.5 1.5 0 0 0 0-3Zm3.5-2a1.5 1.5 0 0 0 0-3H5v3Z",
    italic: "M6 2.75A.75.75 0 0 1 6.75 2h6.5a.75.75 0 0 1 0 1.5h-2.505l-3.858 9H9.25a.75.75 0 0 1 0 1.5h-6.5a.75.75 0 0 1 0-1.5h2.505l3.858-9H6.75A.75.75 0 0 1 6 2.75Z",
    code: "m11.28 3.22 4.25 4.25a.75.75 0 0 1 0 1.06l-4.25 4.25a.749.749 0 0 1-1.275-.326.749.749 0 0 1 .215-.734L13.94 8l-3.72-3.72a.749.749 0 0 1 .326-1.275.749.749 0 0 1 .734.215Zm-6.56 0a.751.751 0 0 1 1.042.018.751.751 0 0 1 .018 1.042L2.06 8l3.72 3.72a.749.749 0 0 1-.326 1.275.749.749 0 0 1-.734-.215L.47 8.53a.75.75 0 0 1 0-1.06Z",
    link: "m7.775 3.275 1.25-1.25a3.5 3.5 0 1 1 4.95 4.95l-2.5 2.5a3.5 3.5 0 0 1-4.95 0 .751.751 0 0 1 .018-1.042.751.751 0 0 1 1.042-.018 1.998 1.998 0 0 0 2.83 0l2.5-2.5a2.002 2.002 0 0 0-2.83-2.83l-1.25 1.25a.751.751 0 0 1-1.042-.018.751.751 0 0 1-.018-1.042Zm-4.69 9.64a1.998 1.998 0 0 0 2.83 0l1.25-1.25a.751.751 0 0 1 1.042.018.751.751 0 0 1 .018 1.042l-1.25 1.25a3.5 3.5 0 1 1-4.95-4.95l2.5-2.5a3.5 3.5 0 0 1 4.95 0 .751.751 0 0 1-.018 1.042.751.751 0 0 1-1.042.018 1.998 1.998 0 0 0-2.83 0l-2.5 2.5a1.998 1.998 0 0 0 0 2.83Z",
    quote: "M1.75 2.5h10.5a.75.75 0 0 1 0 1.5H1.75a.75.75 0 0 1 0-1.5Zm4 5h8.5a.75.75 0 0 1 0 1.5h-8.5a.75.75 0 0 1 0-1.5Zm0 5h8.5a.75.75 0 0 1 0 1.5h-8.5a.75.75 0 0 1 0-1.5ZM2.5 7.75v6a.75.75 0 0 1-1.5 0v-6a.75.75 0 0 1 1.5 0Z",
    "list-unordered": "M5.75 2.5h8.5a.75.75 0 0 1 0 1.5h-8.5a.75.75 0 0 1 0-1.5Zm0 5h8.5a.75.75 0 0 1 0 1.5h-8.5a.75.75 0 0 1 0-1.5Zm0 5h8.5a.75.75 0 0 1 0 1.5h-8.5a.75.75 0 0 1 0-1.5ZM2 14a1 1 0 1 1 0-2 1 1 0 0 1 0 2Zm1-6a1 1 0 1 1-2 0 1 1 0 0 1 2 0ZM2 4a1 1 0 1 1 0-2 1 1 0 0 1 0 2Z",
    heading: "M3.75 2a.75.75 0 0 1 .75.75V7h7V2.75a.75.75 0 0 1 1.5 0v10.5a.75.75 0 0 1-1.5 0V8.5h-7v4.75a.75.75 0 0 1-1.5 0V2.75A.75.75 0 0 1 3.75 2Z",
    "list-ordered": "M5 3.25a.75.75 0 0 1 .75-.75h8.5a.75.75 0 0 1 0 1.5h-8.5A.75.75 0 0 1 5 3.25Zm0 5a.75.75 0 0 1 .75-.75h8.5a.75.75 0 0 1 0 1.5h-8.5A.75.75 0 0 1 5 8.25Zm0 5a.75.75 0 0 1 .75-.75h8.5a.75.75 0 0 1 0 1.5h-8.5a.75.75 0 0 1-.75-.75ZM.924 10.32a.5.5 0 0 1-.851-.525l.001-.001.001-.002.002-.004.007-.011c.097-.144.215-.273.348-.384.228-.19.588-.392 1.068-.392.468 0 .858.181 1.126.484.259.294.377.673.377 1.038 0 .987-.686 1.495-1.156 1.845l-.047.035c-.303.225-.522.4-.654.597h1.357a.5.5 0 0 1 0 1H.5a.5.5 0 0 1-.5-.5c0-1.005.692-1.52 1.167-1.875l.035-.025c.531-.396.8-.625.8-1.078a.57.57 0 0 0-.128-.376C1.806 10.068 1.695 10 1.5 10a.658.658 0 0 0-.429.163.835.835 0 0 0-.144.153ZM2.003 2.5V6h.503a.5.5 0 0 1 0 1H.5a.5.5 0 0 1 0-1h.503V3.308l-.28.14a.5.5 0 0 1-.446-.895l1.003-.5a.5.5 0 0 1 .723.447Z",
    tasklist: "M2 2h4a1 1 0 0 1 1 1v4a1 1 0 0 1-1 1H2a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1Zm4.655 8.595a.75.75 0 0 1 0 1.06L4.03 14.28a.75.75 0 0 1-1.06 0l-1.5-1.5a.749.749 0 0 1 .326-1.275.749.749 0 0 1 .734.215l.97.97 2.095-2.095a.75.75 0 0 1 1.06 0ZM9.75 2.5h5.5a.75.75 0 0 1 0 1.5h-5.5a.75.75 0 0 1 0-1.5Zm0 5h5.5a.75.75 0 0 1 0 1.5h-5.5a.75.75 0 0 1 0-1.5Zm0 5h5.5a.75.75 0 0 1 0 1.5h-5.5a.75.75 0 0 1 0-1.5Zm-7.25-9v3h3v-3Z",
    markdown: "M14.85 3c.63 0 1.15.52 1.14 1.15v7.7c0 .63-.51 1.15-1.15 1.15H1.15C.52 13 0 12.48 0 11.84V4.15C0 3.52.52 3 1.15 3ZM9 11V5H7L5.5 7 4 5H2v6h2V8l1.5 1.92L7 8v3Zm2.99.5L14.5 8H13V5h-2v3H9.5Z",
    smiley: "M8 0a8 8 0 1 1 0 16A8 8 0 0 1 8 0ZM1.5 8a6.5 6.5 0 1 0 13 0 6.5 6.5 0 0 0-13 0Zm3.82 1.636a.75.75 0 0 1 1.038.175l.007.009c.103.118.22.222.35.31.264.178.683.37 1.285.37.602 0 1.02-.192 1.285-.371.13-.088.247-.192.35-.31l.007-.008a.75.75 0 0 1 1.222.87l-.022-.015c.02.013.021.015.021.015v.001l-.001.002-.002.003-.005.007-.014.019a2.066 2.066 0 0 1-.184.213c-.16.166-.338.316-.53.445-.63.418-1.37.638-2.127.629-.946 0-1.652-.308-2.126-.63a3.331 3.331 0 0 1-.715-.657l-.014-.02-.005-.006-.002-.003v-.002h-.001l.613-.432-.614.43a.75.75 0 0 1 .183-1.044ZM12 7a1 1 0 1 1-2 0 1 1 0 0 1 2 0ZM5 8a1 1 0 1 1 0-2 1 1 0 0 1 0 2Zm5.25 2.25.592.416a97.71 97.71 0 0 0-.592-.416Z",
    person: "M10.561 8.073a6.005 6.005 0 0 1 3.432 5.142.75.75 0 1 1-1.498.07 4.5 4.5 0 0 0-8.99 0 .75.75 0 0 1-1.498-.07 6.004 6.004 0 0 1 3.431-5.142 3.999 3.999 0 1 1 5.123 0ZM10.5 5a2.5 2.5 0 1 0-5 0 2.5 2.5 0 0 0 5 0Z",
    kebab: "M8 9a1.5 1.5 0 1 0 0-3 1.5 1.5 0 0 0 0 3ZM1.5 9a1.5 1.5 0 1 0 0-3 1.5 1.5 0 0 0 0 3Zm13 0a1.5 1.5 0 1 0 0-3 1.5 1.5 0 0 0 0 3Z",
    x: "M3.72 3.72a.75.75 0 0 1 1.06 0L8 6.94l3.22-3.22a.749.749 0 0 1 1.275.326.749.749 0 0 1-.215.734L9.06 8l3.22 3.22a.749.749 0 0 1-.326 1.275.749.749 0 0 1-.734-.215L8 9.06l-3.22 3.22a.751.751 0 0 1-1.042-.018.751.751 0 0 1-.018-1.042L6.94 8 3.72 4.78a.75.75 0 0 1 0-1.06Z",
    reply: "M6.78 1.97a.75.75 0 0 1 0 1.06L3.81 6h6.44A4.75 4.75 0 0 1 15 10.75v2.5a.75.75 0 0 1-1.5 0v-2.5a3.25 3.25 0 0 0-3.25-3.25H3.81l2.97 2.97a.749.749 0 0 1-.018 1.042.751.751 0 0 1-1.042.018L1.47 8.53a.75.75 0 0 1 0-1.06l4.25-4.25a.75.75 0 0 1 1.06 0Z",
    copy: "M0 6.75C0 5.784.784 5 1.75 5h1.5a.75.75 0 0 1 0 1.5h-1.5a.25.25 0 0 0-.25.25v7.5c0 .138.112.25.25.25h7.5a.25.25 0 0 0 .25-.25v-1.5a.75.75 0 0 1 1.5 0v1.5A1.75 1.75 0 0 1 9.25 16h-7.5A1.75 1.75 0 0 1 0 14.25Zm5-5C5 .784 5.784 0 6.75 0h7.5C15.216 0 16 .784 16 1.75v7.5A1.75 1.75 0 0 1 14.25 11h-7.5A1.75 1.75 0 0 1 5 9.25Zm1.75-.25a.25.25 0 0 0-.25.25v7.5c0 .138.112.25.25.25h7.5a.25.25 0 0 0 .25-.25v-7.5a.25.25 0 0 0-.25-.25Z",
    pencil: "M11.013 1.427a1.75 1.75 0 0 1 2.474 0l1.086 1.086a1.75 1.75 0 0 1 0 2.474l-8.61 8.61c-.21.21-.47.364-.756.445l-3.251.93a.75.75 0 0 1-.927-.928l.929-3.25c.081-.286.235-.547.445-.758l8.61-8.61Zm.176 4.823L9.75 4.81l-6.286 6.287a.253.253 0 0 0-.064.108l-.558 1.953 1.953-.558a.253.253 0 0 0 .108-.064Zm1.238-3.763a.25.25 0 0 0-.354 0L10.811 3.75l1.439 1.44 1.263-1.263a.25.25 0 0 0 0-.354Z",
    eye: "M8 2c1.981 0 3.671.992 4.933 2.078 1.27 1.091 2.187 2.345 2.637 3.023a1.62 1.62 0 0 1 0 1.798c-.45.678-1.367 1.932-2.637 3.023C11.67 13.008 9.981 14 8 14c-1.981 0-3.671-.992-4.933-2.078C1.797 10.83.88 9.576.43 8.898a1.62 1.62 0 0 1 0-1.798c.45-.677 1.367-1.931 2.637-3.022C4.33 2.992 6.019 2 8 2ZM1.679 7.932a.12.12 0 0 0 0 .136c.411.622 1.241 1.75 2.366 2.717C5.176 11.758 6.527 12.5 8 12.5c1.473 0 2.825-.742 3.955-1.715 1.124-.967 1.954-2.096 2.366-2.717a.12.12 0 0 0 0-.136c-.412-.621-1.242-1.75-2.366-2.717C10.824 4.242 9.473 3.5 8 3.5c-1.473 0-2.825.742-3.955 1.715-1.124.967-1.954 2.096-2.366 2.717ZM8 10a2 2 0 1 1-.001-3.999A2 2 0 0 1 8 10Z",
    "eye-closed": "M.143 2.31a.75.75 0 0 1 1.047-.167l14.5 10.5a.75.75 0 1 1-.88 1.214l-2.248-1.628C11.346 13.19 9.792 14 8 14c-1.981 0-3.67-.992-4.933-2.078C1.797 10.832.88 9.577.43 8.9a1.619 1.619 0 0 1 0-1.797c.353-.533 1.012-1.42 1.918-2.291L.31 3.357A.75.75 0 0 1 .143 2.31Zm1.536 5.622A.12.12 0 0 0 1.657 8c0 .021.006.045.022.068.412.621 1.242 1.75 2.366 2.717C5.175 11.758 6.527 12.5 8 12.5c1.195 0 2.31-.488 3.29-1.191L9.063 9.695A2 2 0 0 1 6.058 7.52L3.708 5.823c-.853.812-1.461 1.633-1.872 2.254a.12.12 0 0 0-.157-.145ZM8 3.5c-.516 0-1.017.09-1.491.251a.75.75 0 1 1-.478-1.422A6.01 6.01 0 0 1 8 2c1.981 0 3.67.992 4.933 2.078 1.27 1.091 2.187 2.345 2.637 3.023a1.62 1.62 0 0 1 0 1.798c-.11.166-.248.365-.41.587a.75.75 0 1 1-1.21-.887c.148-.201.272-.382.371-.531a.12.12 0 0 0 0-.137c-.412-.621-1.242-1.75-2.366-2.717C10.825 4.242 9.473 3.5 8 3.5Z",
    trash: "M11 1.75V3h2.25a.75.75 0 0 1 0 1.5H2.75a.75.75 0 0 1 0-1.5H5V1.75C5 .784 5.784 0 6.75 0h2.5C10.216 0 11 .784 11 1.75ZM4.496 6.675l.66 6.6a.25.25 0 0 0 .249.225h5.19a.25.25 0 0 0 .249-.225l.66-6.6a.75.75 0 0 1 1.492.149l-.66 6.6A1.748 1.748 0 0 1 10.595 15h-5.19a1.75 1.75 0 0 1-1.741-1.575l-.66-6.6a.75.75 0 1 1 1.492-.15ZM6.5 1.75V3h3V1.75a.25.25 0 0 0-.25-.25h-2.5a.25.25 0 0 0-.25.25Z",
    unfold: "m8.177.677 2.896 2.896a.25.25 0 0 1-.177.427H8.75v1.25a.75.75 0 0 1-1.5 0V4H5.104a.25.25 0 0 1-.177-.427L7.823.677a.25.25 0 0 1 .354 0ZM7.25 10.75a.75.75 0 0 1 1.5 0V12h2.146a.25.25 0 0 1 .177.427l-2.896 2.896a.25.25 0 0 1-.354 0l-2.896-2.896A.25.25 0 0 1 5.104 12H7.25v-1.25Zm-5-2a.75.75 0 0 0 0-1.5h-.5a.75.75 0 0 0 0 1.5h.5ZM6 8a.75.75 0 0 1-.75.75h-.5a.75.75 0 0 1 0-1.5h.5A.75.75 0 0 1 6 8Zm2.25.75a.75.75 0 0 0 0-1.5h-.5a.75.75 0 0 0 0 1.5h.5ZM12 8a.75.75 0 0 1-.75.75h-.5a.75.75 0 0 1 0-1.5h.5A.75.75 0 0 1 12 8Zm2.25.75a.75.75 0 0 0 0-1.5h-.5a.75.75 0 0 0 0 1.5h.5Z",
    fold: "M10.896 2H8.75V.75a.75.75 0 0 0-1.5 0V2H5.104a.25.25 0 0 0-.177.427l2.896 2.896a.25.25 0 0 0 .354 0l2.896-2.896A.25.25 0 0 0 10.896 2ZM8.75 15.25a.75.75 0 0 1-1.5 0V14H5.104a.25.25 0 0 1-.177-.427l2.896-2.896a.25.25 0 0 1 .354 0l2.896 2.896a.25.25 0 0 1-.177.427H8.75v1.25Zm-6.5-6.5a.75.75 0 0 0 0-1.5h-.5a.75.75 0 0 0 0 1.5h.5ZM6 8a.75.75 0 0 1-.75.75h-.5a.75.75 0 0 1 0-1.5h.5A.75.75 0 0 1 6 8Zm2.25.75a.75.75 0 0 0 0-1.5h-.5a.75.75 0 0 0 0 1.5h.5ZM12 8a.75.75 0 0 1-.75.75h-.5a.75.75 0 0 1 0-1.5h.5A.75.75 0 0 1 12 8Zm2.25.75a.75.75 0 0 0 0-1.5h-.5a.75.75 0 0 0 0 1.5h.5Z",
  };

  function octicon(name) {
    return '<svg class="gc-oct" viewBox="0 0 16 16" width="16" height="16" aria-hidden="true">' +
      '<path fill="currentColor" d="' + OCTICONS[name] + '"></path></svg>';
  }

  // Copy text to the clipboard, falling back to a hidden textarea where the async
  // Clipboard API isn't available (older browsers, or non-secure contexts).
  function copyText(text) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      return navigator.clipboard.writeText(text);
    }
    return new Promise(function (resolve) {
      const ta = document.createElement("textarea");
      ta.value = text; ta.style.position = "fixed"; ta.style.opacity = "0";
      document.body.appendChild(ta); ta.focus(); ta.select();
      try { document.execCommand("copy"); } catch (_) {}
      document.body.removeChild(ta); resolve();
    });
  }

  // Wrap the selection with before/after, or strip them if they're already there, so
  // the same button toggles formatting off (like GitHub). Uses replaceRange so the
  // edit stays on the textarea's native undo stack.
  function toggleWrap(ta, before, after) {
    const s = ta.selectionStart, e = ta.selectionEnd, v = ta.value;
    const sel = v.slice(s, e);
    // Markers already inside the selection -> unwrap.
    if (sel.length >= before.length + after.length &&
        sel.startsWith(before) && sel.endsWith(after)) {
      const inner = sel.slice(before.length, sel.length - after.length);
      replaceRange(ta, s, e, inner, s, s + inner.length);
      return;
    }
    // Markers sitting just outside the selection -> unwrap.
    if (before && v.slice(s - before.length, s) === before &&
        v.slice(e, e + after.length) === after) {
      replaceRange(ta, s - before.length, e + after.length, sel,
                   s - before.length, s - before.length + sel.length);
      return;
    }
    const p = s + before.length;
    replaceRange(ta, s, e, before + sel + after, p, p + sel.length);
  }

  // Code button: inline `code` (toggleable) for a single-line selection, or a fenced
  // ``` block on its own lines when the selection spans multiple lines.
  function applyCode(ta) {
    const s = ta.selectionStart, e = ta.selectionEnd, v = ta.value;
    const sel = v.slice(s, e);
    if (sel.indexOf("\n") === -1) { toggleWrap(ta, "`", "`"); return; }
    const padBefore = s > 0 && v[s - 1] !== "\n" ? "\n" : "";
    const padAfter = e < v.length && v[e] !== "\n" ? "\n" : "";
    const block = padBefore + "```\n" + sel + "\n```" + padAfter;
    const inner = s + padBefore.length + 4; // past the leading ```\n
    replaceRange(ta, s, e, block, inner, inner + sel.length);
  }

  // Link button: a URL-looking selection goes into the (url) slot (caret on the text
  // placeholder); otherwise the selection is the link text (caret on the url
  // placeholder). An empty selection drops the caret inside the [] label.
  function applyLink(ta) {
    const s = ta.selectionStart, e = ta.selectionEnd, v = ta.value;
    const sel = v.slice(s, e);
    if (sel && /^(https?:\/\/|mailto:|www\.)\S*$/i.test(sel.trim())) {
      const text = "text";
      replaceRange(ta, s, e, "[" + text + "](" + sel.trim() + ")", s + 1, s + 1 + text.length);
    } else if (sel) {
      const urlAt = s + 1 + sel.length + 2; // past "[sel]("
      replaceRange(ta, s, e, "[" + sel + "](url)", urlAt, urlAt + 3);
    } else {
      replaceRange(ta, s, e, "[](url)", s + 1, s + 1);
    }
  }

  // Insert text at the caret on its own line(s) (used by the GIF picker to drop an
  // image). Pads with newlines so the image isn't glued to surrounding prose.
  function insertAtCursor(ta, text) {
    const s = ta.selectionStart, e = ta.selectionEnd, v = ta.value;
    const padBefore = s > 0 && v[s - 1] !== "\n" ? "\n" : "";
    const padAfter = e < v.length && v[e] !== "\n" ? "\n" : "";
    const block = padBefore + text + padAfter;
    const caret = s + block.length;
    replaceRange(ta, s, e, block, caret, caret);
  }

  // Toggle a block marker (list/quote/heading) on the selected line(s): add it to
  // every line, or strip it if all lines already have it. Lists/quotes pad with a
  // blank line so they're separated from non-empty neighbors (valid Markdown);
  // headings don't pad (ATX headings are fine inline, and we must not add new lines).
  function replaceRange(ta, start, end, text, selA, selB) {
    ta.focus();
    ta.setSelectionRange(start, end);
    let ok = false;
    try { ok = document.execCommand("insertText", false, text); } catch (_) { ok = false; }
    if (!ok) ta.value = ta.value.slice(0, start) + text + ta.value.slice(end);
    if (selA == null) { selA = start; selB = start + text.length; }
    ta.setSelectionRange(selA, selB == null ? selA : selB);
  }

  function toggleLinePrefix(ta, marker) {
    const ordered = /^\d+\.\s/.test(marker);
    const task = /\[\s?\]/.test(marker);
    const quote = /^>\s/.test(marker);
    const heading = /^#+\s/.test(marker);
    const isList = ordered || task || (!quote && !heading); // unordered = the default
    // Which marker counts as "already this type" (decides toggle-off).
    const targetRe = heading ? /^#{1,6}\s/
                   : ordered ? /^\d+\.\s/
                   : task ? /^[-*]\s\[[ xX]\]\s/
                   : quote ? /^>\s/
                   : /^[-*]\s(?!\[[ xX]\]\s)/;
    // What to strip before applying. List types cross-convert, so strip ANY list
    // marker (same type → removed below; different type → replaced). Quote/heading
    // only strip their own kind.
    const stripRe = isList ? /^(?:\d+\.\s|[-*]\s\[[ xX]\]\s|[-*]\s)/ : targetRe;

    const v = ta.value, selS = ta.selectionStart, selE = ta.selectionEnd;
    const start = v.lastIndexOf("\n", selS - 1) + 1;
    let end = v.indexOf("\n", selE);
    if (end === -1) end = v.length;
    const lines = v.slice(start, end).split("\n");
    const nonEmpty = lines.filter((l) => l.trim() !== "");
    // Toggle off only when every non-empty line is already exactly this type.
    const remove = nonEmpty.length > 0 && nonEmpty.every((l) => targetRe.test(l));
    // When the whole target is blank (e.g. clicking on an empty line) still add the
    // marker; otherwise blank lines are separators between items and are left alone.
    const onlyEmpty = nonEmpty.length === 0;

    let n = 0;
    const oldPre = [], newPre = []; // per-line prefix lengths (stripped vs added)
    const newLines = lines.map((l) => {
      if (l.trim() === "" && !onlyEmpty) { oldPre.push(0); newPre.push(0); return l; }
      const m = l.match(stripRe);
      const op = m ? m[0].length : 0;
      const bare = l.slice(op);
      if (remove) { oldPre.push(op); newPre.push(0); return bare; }
      n++;
      const np = ordered ? n + ". " : marker;
      oldPre.push(op); newPre.push(np.length);
      return np + bare;
    });
    let block = newLines.join("\n");

    // Lists/quotes pad with a blank line when added; headings never add lines.
    let pad = 0;
    if (!remove && !heading) {
      const head = v.slice(0, start), tail = v.slice(end);
      if (head.endsWith("\n") && !head.endsWith("\n\n")) { block = "\n" + block; pad = 1; }
      if (tail.startsWith("\n") && !tail.startsWith("\n\n")) block = block + "\n";
    }

    // Map an original position to its new spot so the caret/selection is preserved
    // relative to the text (only line-start markers changed length).
    function mapPos(p) {
      let acc = start + pad, off = p - start;
      for (let i = 0; i < lines.length; i++) {
        if (off <= lines[i].length) return acc + newPre[i] + Math.max(0, off - oldPre[i]);
        off -= lines[i].length + 1;
        acc += newLines[i].length + 1;
      }
      return acc;
    }
    replaceRange(ta, start, end, block, mapPos(selS), mapPos(selE));
  }

  function relTime(iso) {
    const then = new Date(iso), now = new Date();
    const s = Math.round((now - then) / 1000);
    const u = [["year", 31536000], ["month", 2592000], ["day", 86400], ["hour", 3600], ["minute", 60]];
    for (const [name, secs] of u) {
      const n = Math.floor(s / secs);
      if (n >= 1) return n + " " + name + (n > 1 ? "s" : "") + " ago";
    }
    return "just now";
  }

  function el(tag, cls, text) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text != null) e.textContent = text;
    return e;
  }

  // "edited" tag for the meta row: shown once a comment has changed after posting
  // (updatedAt is set by edit_comment). Title says when. null if never edited.
  function editedTag(c) {
    if (!c.updatedAt) return null;
    const tag = el("span", "gc-edited", "edited");
    tag.title = "Edited " + relTime(c.updatedAt);
    return tag;
  }

  // Friendly handle for display: drop the configured login suffix (e.g. an EMU
  // "_Acme" → the bare handle). The FULL login stays the identity key
  // everywhere for ownership and admin checks. This only changes what's shown.
  function displayHandle(login) {
    const suf = (CFG && CFG.stripSuffix) || "";
    if (suf && login && login.length > suf.length && login.endsWith(suf)) {
      return login.slice(0, -suf.length);
    }
    return login || "";
  }

  // POST a reaction toggle to the backend, which reacts on GitHub using the reader's
  // session token. Returns the subject's updated reaction groups (with viewerHasReacted).
  async function sendReaction(commentId, content, on) {
    const r = await api(CFG, "/api/react", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ comment_id: commentId, content: content, on: on }),
    });
    if (!r.ok) {
      let msg = "HTTP " + r.status;
      try { const j = await r.json(); if (j && j.detail != null) msg = typeof j.detail === "string" ? j.detail : JSON.stringify(j.detail); } catch (_) {}
      throw new Error(msg);
    }
    return await r.json(); // { comment_id, reactions: [{content, count, viewerHasReacted}] }
  }

  // The reaction bar under a comment: count pills plus, when signed in, an "add
  // reaction" picker. Pills are clickable (toggle) only when signed in. Reads use
  // the server token, so viewerHasReacted isn't known until the reader acts; the
  // /api/react response then fills it in for that comment.
  function buildReactions(c, readonly) {
    const state = new Map();
    for (const r of (c.reactions || [])) {
      state.set(r.content, { count: r.count, viewerHasReacted: !!r.viewerHasReacted });
    }
    const bar = el("div", "gc-reactions");
    // A hidden comment is read-only: show its reaction counts, but no add-picker and
    // no toggling.
    const interactive = SIGNED_IN && !readonly;

    async function toggle(content, on) {
      try {
        const res = await sendReaction(c.id, content, on);
        state.clear();
        for (const r of (res.reactions || [])) {
          state.set(r.content, { count: r.count, viewerHasReacted: !!r.viewerHasReacted });
        }
        render();
      } catch (e) {
        alert("Could not react: " + e.message);
      }
    }

    function addPicker() {
      const wrap = el("span", "gc-react-add-wrap");
      const add = el("button", "gc-react-add"); add.type = "button";
      add.innerHTML = octicon("smiley");
      add.title = "Add reaction"; add.tabIndex = -1;
      const pop = el("div", "gc-react-pop"); pop.hidden = true;
      for (const content of REACTION_ORDER) {
        const opt = el("button", "gc-react-opt", REACTION[content]);
        opt.type = "button"; opt.title = content.toLowerCase().replace(/_/g, " ");
        opt.addEventListener("click", () => {
          pop.hidden = true;
          const s = state.get(content);
          toggle(content, !(s && s.viewerHasReacted));
        });
        pop.appendChild(opt);
      }
      add.addEventListener("click", () => {
        pop.hidden = !pop.hidden;
        if (pop.hidden) return;
        // Close on the next outside click, then detach the listener.
        const close = (e) => {
          if (!wrap.contains(e.target)) { pop.hidden = true; document.removeEventListener("click", close); }
        };
        setTimeout(() => document.addEventListener("click", close), 0);
      });
      wrap.append(add, pop);
      return wrap;
    }

    function render() {
      bar.innerHTML = "";
      if (interactive) bar.appendChild(addPicker());
      for (const content of REACTION_ORDER) {
        const s = state.get(content);
        if (!s || !s.count) continue;
        const pill = el("button", "gc-reaction" + (s.viewerHasReacted ? " gc-reacted" : ""),
          (REACTION[content] || "•") + " " + s.count);
        pill.type = "button";
        if (interactive) pill.addEventListener("click", () => toggle(content, !s.viewerHasReacted));
        else pill.disabled = true;
        bar.appendChild(pill);
      }
    }

    render();
    return bar;
  }

  // The ⋯ options menu on a comment. Always offers Quote reply (signed-in), Copy
  // link, and Copy text; Edit (author only), Hide/Unhide (admin), and Delete (author
  // or admin) are added when allowed. The backend re-checks every mutation, so the
  // client-side gating is purely cosmetic.
  function buildCommentMenu(c, ctx) {
    const wrap = el("span", "gc-menu-wrap");
    const btn = el("button", "gc-menu-btn"); btn.type = "button";
    btn.title = "Comment options"; btn.setAttribute("aria-label", "Comment options");
    btn.innerHTML = octicon("kebab");
    const menu = el("div", "gc-menu"); menu.hidden = true;

    // Lift the whole comment above its siblings while the menu is open so the
    // dropdown can't be painted over by later comments (esp. minimized ones).
    function close() { menu.hidden = true; if (ctx.row) ctx.row.classList.remove("gc-menu-open"); }
    function clear() { menu.innerHTML = ""; }
    function additem(icon, label, onClick, opts) {
      opts = opts || {};
      const it = el("button", "gc-menu-item" + (opts.cls ? " " + opts.cls : "")); it.type = "button";
      it.innerHTML = (icon ? octicon(icon) : "") + "<span>" + label + "</span>";
      it.addEventListener("click", function (e) {
        // Don't let the click reach the outside-click handler: re-rendering the menu
        // (e.g. Hide -> reasons) detaches this button, which would otherwise look
        // "outside" the menu and close it.
        e.stopPropagation();
        if (!opts.keepOpen) close();
        onClick();
      });
      menu.appendChild(it);
      return it;
    }
    function sep() { menu.appendChild(el("div", "gc-menu-sep")); }

    const mine = SIGNED_IN && ME && ME.login && c.authorLogin && ME.login === c.authorLogin;
    const admin = SIGNED_IN && ME && ME.isAdmin;

    function renderMain() {
      clear();
      // No quote-reply on a hidden comment, nor on a reply whose parent is hidden
      // because the thread is read-only and has no reply box to open.
      const parentHidden = !!(ctx.topCtx && ctx.topCtx.comment && ctx.topCtx.comment.isMinimized);
      if (SIGNED_IN && !c.isMinimized && !parentHidden) {
        additem("reply", "Quote reply", function () {
          const md = (c.bodyMarkdown != null ? c.bodyMarkdown : "").replace(/\s+$/, "");
          const quoted = (md ? md.split("\n").map(function (l) { return "> " + l; }).join("\n") : "> ") + "\n\n";
          const target = (ctx.topCtx && ctx.topCtx.bubble) || ctx.bubble;
          if (target && target.gcOpenReply) target.gcOpenReply(quoted);
        });
      }
      additem("link", "Copy link", function () {
        const base = (CFG && CFG.url) || (location.origin + location.pathname);
        copyText(base.split("#")[0] + "#" + c.id);
      });
      additem("copy", "Copy text", function () {
        copyText(c.bodyMarkdown != null ? c.bodyMarkdown : "");
      });
      if (mine || admin) sep();
      if (mine || admin) additem("pencil", "Edit", function () { startEdit(c, ctx); });
      if (admin) {
        if (c.isMinimized) additem("eye", "Unhide", function () { hideComment(c, false, null, ctx); });
        // Hiding asks for a reason: swap to a reason submenu rather than closing.
        else additem("eye-closed", "Hide", function () { renderHideReasons(); }, { keepOpen: true });
      }
      if (mine || admin) additem("trash", "Delete", function () { deleteComment(c, ctx); }, { cls: "gc-menu-danger" });
    }

    function renderHideReasons() {
      clear();
      menu.appendChild(el("div", "gc-menu-label", "Hide comment as:"));
      for (const pair of HIDE_REASONS) {
        (function (value) {
          additem(null, pair[2] + "  " + pair[1], function () { hideComment(c, true, value, ctx); });
        })(pair[0]);
      }
      sep();
      additem(null, "← Back", function () { renderHideReasons.back(); }, { keepOpen: true });
    }
    renderHideReasons.back = renderMain;

    btn.addEventListener("click", function () {
      if (!menu.hidden) { close(); return; }
      renderMain();
      menu.hidden = false;
      if (ctx.row) ctx.row.classList.add("gc-menu-open");
      const onDoc = function (e) {
        if (!wrap.contains(e.target)) { close(); document.removeEventListener("click", onDoc); }
      };
      setTimeout(function () { document.addEventListener("click", onDoc); }, 0);
    });
    wrap.append(btn, menu);
    return wrap;
  }

  // Replace a comment's body with an inline editor (reusing the composer in edit
  // mode), prefilled with its raw Markdown. Save calls the edit endpoint and swaps
  // the rendered body back in; Cancel restores the original body element.
  function startEdit(c, ctx) {
    if (ctx.expand) ctx.expand(); // reveal a hidden comment's body before editing it
    const original = ctx.bodyEl;
    const composer = buildComposer(ROOT, CFG, ME, {
      editMode: true,
      initialValue: c.bodyMarkdown != null ? c.bodyMarkdown : "",
      onSubmit: async function (body) {
        const r = await api(CFG, "/api/comments/edit", {
          method: "POST", headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ comment_id: c.id, body: body }),
        });
        if (r.status === 401) throw new Error("Please sign in.");
        if (r.status === 403) throw new Error("You can only edit your own comment.");
        if (!r.ok) throw new Error("HTTP " + r.status);
        return await r.json();
      },
      onPosted: function (updated) {
        c.bodyHTML = updated.bodyHTML;
        c.bodyMarkdown = updated.bodyMarkdown;
        c.updatedAt = updated.updatedAt;
        const fresh = el("div", "gc-body");
        fresh.innerHTML = updated.bodyHTML;
        composer.replaceWith(fresh);
        ctx.bodyEl = fresh;
        // Reflect the new "edited" state in the meta without a full re-render.
        // Insert right after the timestamp (the ⋯ menu is the meta's last child, so a
        // plain append would land after it).
        const meta = ctx.row.querySelector(".gc-meta");
        if (meta) {
          const old = meta.querySelector(".gc-edited");
          if (old) old.remove();
          const et = editedTag(c);
          if (et) {
            const timeEl = meta.querySelector(".gc-time");
            meta.insertBefore(et, timeEl ? timeEl.nextSibling : null);
          }
        }
      },
      onCancel: function () { composer.replaceWith(original); ctx.bodyEl = original; },
    });
    original.replaceWith(composer);
    const ta = composer.querySelector(".gc-textarea");
    if (ta) { ta.focus(); ta.setSelectionRange(ta.value.length, ta.value.length); }
  }

  async function hideComment(c, hide, reason, ctx) {
    try {
      const payload = { comment_id: c.id, hide: hide };
      if (hide && reason) payload.reason = reason;
      const r = await api(CFG, "/api/comments/hide", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      if (r.status === 403) { alert("Moderators only."); return; }
      if (!r.ok) throw new Error("HTTP " + r.status);
      // Re-render just this comment in place (collapsed <-> expanded) instead of
      // reloading the whole list. Keep BOTH the minimized flag and its reason in sync
      // (`reason` is already the canonical code, matching what the server stores), so a
      // re-hide with a new reason doesn't show the previous one until a refresh.
      c.isMinimized = hide;
      c.minimizedReason = hide ? (reason || null) : null;
      c.hiddenAt = hide ? new Date().toISOString() : null;
      const fresh = renderComment(c, ctx.isReply, ctx.topCtx);
      ctx.row.replaceWith(fresh);
    } catch (e) {
      alert("Could not " + (hide ? "hide" : "unhide") + ": " + e.message);
    }
  }

  async function deleteComment(c, ctx) {
    if (!confirm("Delete this comment? This cannot be undone.")) return;
    try {
      const r = await api(CFG, "/api/comments/delete", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ comment_id: c.id }),
      });
      if (r.status === 403) { alert("You can't delete this comment."); return; }
      if (!r.ok) throw new Error("HTTP " + r.status);
      // Optimistic: drop the comment from the DOM without reloading. The header counts
      // top-level comments only, so a top-level delete is -1 and a reply delete leaves
      // it unchanged.
      const repliesBlock = (ctx.row.parentNode && ctx.row.parentNode.classList &&
        ctx.row.parentNode.classList.contains("gc-replies")) ? ctx.row.parentNode : null;
      ctx.row.remove();
      if (!ctx.isReply) {
        const n = parseInt(ROOT.dataset.gcTotal || "0", 10) || 0;
        setCount(ROOT, Math.max(0, n - 1));
        const list = ROOT.querySelector(".gc-list");
        if (list && !list.querySelector(".gc-comment")) {
          list.appendChild(el("div", "gc-empty", "No comments yet. Be the first."));
        }
      } else if (repliesBlock && !repliesBlock.querySelector(".gc-comment") &&
                 !repliesBlock.querySelector(".gc-replies-more")) {
        repliesBlock.remove(); // removed the last reply, and nothing overflowed to GitHub
      }
    } catch (e) {
      alert("Could not delete: " + e.message);
    }
  }

  function renderComment(c, isReply, topCtx) {
    const row = el("article", "gc-comment" + (isReply ? " gc-reply" : "") +
      (c.isMinimized ? " gc-minimized" : ""));
    row.id = c.id; // anchor target for "Copy link"
    const av = el("img", "gc-avatar");
    av.src = c.author.avatarUrl; av.alt = c.author.login; av.loading = "lazy";
    row.appendChild(av);

    const bubble = el("div", "gc-bubble");
    const ctx = { row: row, bubble: bubble, isReply: !!isReply, topCtx: topCtx, bodyEl: null, expand: null };
    const menu = buildCommentMenu(c, ctx);

    // Author/time row (the ⋯ menu is added here for normal comments; for hidden ones
    // it lives in the collapse bar instead). The login is shown via displayHandle()
    // (e.g. drops the EMU "_Acme" suffix); the full login stays the identity key.
    const meta = el("div", "gc-meta");
    const handle = displayHandle(c.author.login);
    const realName = c.author.name && c.author.name !== c.author.login ? c.author.name : null;
    const author = el("a", "gc-author", realName || handle);
    author.href = c.author.url; author.target = "_blank"; author.rel = "noopener";
    meta.appendChild(author);
    if (realName) {
      meta.appendChild(el("span", "gc-handle", "(" + handle + ")"));
    }
    meta.append(el("span", null, "commented"));
    const time = el("a", "gc-time", relTime(c.createdAt));
    time.href = c.url; time.title = c.createdAt; time.target = "_blank"; time.rel = "noopener";
    meta.appendChild(time);
    const edited = editedTag(c);
    if (edited) meta.appendChild(edited);

    const body = el("div", "gc-body");
    body.innerHTML = c.bodyHTML; // GitHub-sanitized HTML
    ctx.bodyEl = body;
    const rx = buildReactions(c, c.isMinimized);

    if (c.isMinimized) {
      // Collapse only the parent's own content (meta/body/reactions) behind the bar.
      // The ⋯ menu lives in the meta (like a normal comment), so it's revealed when
      // you expand the comment. Replies stay visible below. Hiding a comment
      // shouldn't bury the thread that answered it. No reply footer, though: you
      // can't add a new reply to a hidden comment.
      meta.appendChild(menu);
      const content = el("div", "gc-hidden-content"); content.hidden = true;
      content.append(meta, body);
      if (rx.childElementCount) content.appendChild(rx);
      const bar = el("div", "gc-hidden-bar");
      // GitHub-style sentence with the time on the verb so it reads for every reason:
      // "This comment was marked as spam 3 days ago." / "...was resolved 2 hours ago."
      // No emoji/pill/parens. Falls back when there's no reason or (old rows) no time.
      const hr = hideReason(c.minimizedReason);
      const when = c.hiddenAt ? relTime(c.hiddenAt) : null;
      let htext = hr ? ("This comment was " + hr.phrase) : "This comment was hidden";
      if (when) htext += " " + when;
      htext += ".";
      const htextEl = el("span", "gc-hidden-text", htext);
      if (c.hiddenAt) htextEl.title = "Hidden on " + new Date(c.hiddenAt).toLocaleString();
      bar.appendChild(htextEl);
      const toggle = el("button", "gc-hidden-toggle"); toggle.type = "button";
      function paint() {
        toggle.innerHTML = octicon(content.hidden ? "unfold" : "fold") +
          "<span>" + (content.hidden ? "Show comment" : "Hide comment") + "</span>";
      }
      ctx.expand = function () { content.hidden = false; paint(); };
      paint();
      // The whole bar toggles show/hide.
      bar.addEventListener("click", function () { content.hidden = !content.hidden; paint(); });
      bar.appendChild(toggle);
      bubble.append(bar, content);
      if (!isReply) {
        const replies = buildRepliesWrap(c, bubble);
        if (replies) bubble.appendChild(replies); // stay visible; no reply footer
      }
    } else {
      meta.appendChild(menu);
      bubble.appendChild(meta);
      bubble.appendChild(body);
      if (rx.childElementCount) bubble.appendChild(rx);
      if (!isReply) {
        // Nested replies + the reply editor footer live INSIDE the bubble, so the
        // whole thread reads as one card and the editor sits below the replies.
        const replies = buildRepliesWrap(c, bubble);
        if (replies) bubble.appendChild(replies);
        if (SIGNED_IN) bubble.appendChild(buildReplyFooter(c, bubble));
      }
    }
    row.appendChild(bubble);
    return row;
  }

  function setCount(root, n) {
    root.dataset.gcTotal = String(n);
    const count = root.querySelector(".gc-count");
    if (count) count.textContent = n + (n === 1 ? " comment" : " comments");
  }

  async function fetchPage(cfg, after) {
    const path = "/api/discussions?term=" + encodeURIComponent(cfg.term) +
      (after ? "&after=" + encodeURIComponent(after) : "");
    const r = await api(cfg, path); // credentials:include so the session cookie is sent
    if (!r.ok) throw new Error("HTTP " + r.status);
    return await r.json();
  }

  // Existing replies under a top-level comment (+ an overflow link to GitHub). The
  // reply editor itself lives in the comment bubble's footer, not here.
  function buildRepliesWrap(c, topBubble) {
    if (!c.replies || !c.replies.length) return null;
    const wrap = el("div", "gc-replies");
    const topCtx = { comment: c, bubble: topBubble };
    for (const rep of c.replies) wrap.appendChild(renderComment(rep, true, topCtx));
    // GitHub caps replies we fetch per comment; link the overflow out rather
    // than silently hiding it.
    if (c.repliesHaveMore) {
      const extra = (c.replyCount || c.replies.length) - c.replies.length;
      const more = el("a", "gc-replies-more",
        "Show " + (extra > 0 ? extra + " more " : "more ") +
        (extra === 1 ? "reply" : "replies") + " on GitHub →");
      more.href = c.url; more.target = "_blank"; more.rel = "noopener";
      wrap.appendChild(more);
    }
    return wrap;
  }

  // Reply editor as a footer inside the comment's bubble: a collapsed "Write a
  // reply…" stub that expands into the full composer (reply mode). New replies are
  // appended to the bubble's .gc-replies block (created on demand), just above this
  // footer, so they stay inside the same card.
  function buildReplyFooter(c, bubble) {
    const footer = el("div", "gc-reply-footer");
    const stub = el("div", "gc-reply-stub", "Write a reply…");
    stub.tabIndex = 0; stub.setAttribute("role", "button");
    footer.appendChild(stub);
    function insertReply(created) {
      let block = bubble.querySelector(":scope > .gc-replies");
      if (!block) { block = el("div", "gc-replies"); bubble.insertBefore(block, footer); }
      block.appendChild(renderComment(created, true, { comment: c, bubble: bubble }));
    }
    let composer = null;
    function closeComposer() {
      if (composer) { composer.replaceWith(stub); composer = null; }
    }
    // open(initialValue?): expand the reply composer (creating it if needed). If it's
    // already open, prepend the quoted text and refocus (used by "Quote reply").
    function open(initialValue) {
      if (composer) {
        const ta0 = composer.querySelector(".gc-textarea");
        if (ta0) {
          if (initialValue) ta0.value = initialValue + (ta0.value ? "\n" + ta0.value : "");
          ta0.focus(); ta0.setSelectionRange(ta0.value.length, ta0.value.length);
        }
        return;
      }
      composer = buildComposer(ROOT, CFG, ME, {
        replyToId: c.id,
        initialValue: initialValue || "",
        onCancel: closeComposer,
        onPosted: function (created) {
          if (created && created.id) insertReply(created);
          closeComposer();
        },
      });
      stub.replaceWith(composer);
      const ta = composer.querySelector(".gc-textarea");
      if (ta) { ta.focus(); ta.setSelectionRange(ta.value.length, ta.value.length); }
    }
    stub.addEventListener("click", function () { open(); });
    stub.addEventListener("keydown", function (e) {
      if (e.key === "Enter" || e.key === " ") { e.preventDefault(); open(); }
    });
    bubble.gcOpenReply = open; // so the ⋯ "Quote reply" can open this footer prefilled
    return footer;
  }

  // Append one page of comments (each renders its own nested replies + reply
  // footer inside its bubble), then (re)place the "Load more" button last.
  function renderPage(list, cfg, data) {
    const old = list.querySelector(".gc-loadmore");
    if (old) old.remove();
    for (const c of data.comments) {
      list.appendChild(renderComment(c, false));
    }
    const pi = data.pageInfo || {};
    if (pi.hasNextPage) {
      const btn = el("button", "gc-loadmore", "Load more comments");
      btn.type = "button";
      btn.addEventListener("click", async () => {
        btn.disabled = true; btn.textContent = "Loading…";
        try {
          renderPage(list, cfg, await fetchPage(cfg, pi.endCursor));
        } catch (e) {
          btn.disabled = false; btn.textContent = "Load more comments";
          alert("Could not load more: " + e.message);
        }
      });
      list.appendChild(btn);
    }
  }

  // After a successful post, show the new comment immediately (before any
  // trailing "Load more" button) instead of reloading from page 1.
  function appendNewComment(root, c) {
    const list = root.querySelector(".gc-list");
    const empty = list.querySelector(".gc-empty");
    if (empty) empty.remove();
    const node = renderComment(c, false);
    const btn = list.querySelector(".gc-loadmore");
    if (btn) list.insertBefore(node, btn); else list.appendChild(node);
    setCount(root, (parseInt(root.dataset.gcTotal || "0", 10) || 0) + 1);
  }

  async function load(root, cfg) {
    const list = root.querySelector(".gc-list");
    list.innerHTML = "";
    let data;
    try {
      data = await fetchPage(cfg, null);
    } catch (e) {
      list.appendChild(el("div", "gc-empty", "Could not load comments (" + e.message + ")."));
      return;
    }
    setCount(root, data.discussion.totalCount);
    if (!data.comments.length) {
      list.appendChild(el("div", "gc-empty", "No comments yet. Be the first."));
      return;
    }
    renderPage(list, cfg, data);
    focusHashComment(root);
  }

  // If the URL points at a specific comment (…#<comment-id>, from "Copy link"),
  // scroll it into view and flash a highlight. Best-effort: only finds comments on
  // the currently-loaded page.
  function focusHashComment(root) {
    const id = (location.hash || "").slice(1);
    if (!id) return;
    const target = root.querySelector('[id="' + id.replace(/["\\]/g, "") + '"]');
    if (!target) return;
    target.scrollIntoView({ behavior: "smooth", block: "center" });
    target.classList.add("gc-target");
    setTimeout(function () { target.classList.remove("gc-target"); }, 2400);
  }

  // The session token (identity, HMAC-signed) lives in localStorage so sign-in works without
  // a cross-site cookie, which mobile Safari blocks. It rides as an Authorization: Bearer
  // header on every API call; the cross-site cookie stays a same-site/desktop fallback.
  function gcToken(cfg, val) {
    var key = "gc:token:" + (cfg.backend || "same-origin");
    try {
      if (arguments.length === 1) return localStorage.getItem(key) || "";
      if (val == null) { localStorage.removeItem(key); return ""; }
      localStorage.setItem(key, val); return val;
    } catch (_) { return ""; }
  }

  function api(cfg, path, opts) {
    opts = opts || {};
    var headers = Object.assign({}, opts.headers);
    var tok = gcToken(cfg);
    if (tok) headers["Authorization"] = "Bearer " + tok;
    if (cfg.accessKey) headers["X-Discussions-Key"] = cfg.accessKey;
    return fetch((cfg.backend || "") + path, Object.assign({ credentials: "include" }, opts, { headers: headers }));
  }

  async function fetchMe(cfg) {
    try {
      const r = await api(cfg, "/api/me");
      if (!r.ok) return { authenticated: false, oauth: false };
      return await r.json();
    } catch (_) {
      return { authenticated: false, oauth: false };
    }
  }

  // Server-driven per-tenant config (multi-tenant): the tenant base URL selects this
  // blog's widget config, and the backend also verifies the request Origin. Values provided via data
  // attributes win; we only fill what's missing, so a fully-configured blog never fetches.
  async function fetchConfig(cfg) {
    try {
      const r = await api(cfg, "/config");
      if (!r.ok) return;
      const c = await r.json();
      const widget = c.widget || c;
      if (!cfg.stripSuffix && widget.stripSuffix) cfg.stripSuffix = widget.stripSuffix;
      if (!cfg.giphyKey && widget.giphyKey) cfg.giphyKey = widget.giphyKey;
    } catch (_) {}
  }

  async function openLogin(cfg) {
    // Ask the Worker for the provider URL through fetch so a protected tenant can validate
    // its access key without placing it in browser history, Worker URLs, or OAuth state.
    const r = await api(cfg, "/auth/login?return=" + encodeURIComponent(location.href), { method: "POST" });
    if (!r.ok) throw new Error("HTTP " + r.status);
    const body = await r.json();
    if (!body.url) throw new Error("missing authorization URL");
    location.href = body.url;
  }

  // On load, pick up a token handed back by the OAuth redirect (#gc_token=...), stash it, and
  // strip it from the URL so it isn't left in history or the address bar.
  function absorbAuthToken(cfg) {
    var h = location.hash || "";
    var m = h.match(/[#&]gc_token=([^&]*)/);
    if (!m) return;
    gcToken(cfg, decodeURIComponent(m[1]));
    var rest = h.replace(/[#&]gc_token=[^&]*/, "");
    if (rest === "#") rest = "";
    try { history.replaceState(null, "", location.pathname + location.search + rest); } catch (_) {}
  }

  function submitter(root, cfg, opts) {
    opts = opts || {};
    const label = opts.editMode ? "Update comment" : (opts.replyToId ? "Reply" : "Comment");
    return async function (ta, submit) {
      const body = ta.value.trim();
      if (!body) return;
      submit.disabled = true; submit.textContent = opts.editMode ? "Updating…" : "Posting…";
      try {
        let created;
        if (opts.onSubmit) {
          // Edit (or any custom action): the caller owns the request + result.
          created = await opts.onSubmit(body);
        } else {
          const payload = { body: body, term: cfg.term };
          // Page metadata, stored with the thread the first time it's seen.
          if (cfg.title) payload.title = cfg.title;
          if (cfg.subtitle) payload.subtitle = cfg.subtitle;
          if (cfg.url) payload.url = cfg.url;
          if (opts.replyToId) payload.reply_to_id = opts.replyToId;
          const r = await api(cfg, "/api/comments", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload),
          });
          if (r.status === 401) { alert("Please sign in to comment."); return; }
          if (!r.ok) throw new Error("HTTP " + r.status);
          created = await r.json().catch(function () { return null; });
          ta.value = "";
        }
        if (opts.onPosted) opts.onPosted(created);
        else if (created && created.id) appendNewComment(root, created);
        else await load(root, cfg);
      } catch (e) {
        alert("Could not " + (opts.editMode ? "update" : "post") + ": " + e.message);
      } finally {
        submit.disabled = false; submit.textContent = label;
      }
    };
  }

  // Client-side GIPHY picker (no backend): an inline panel with a search box + a grid
  // of results. Picking one inserts a markdown image of the GIF at the caret, which
  // The server renders this as an <img> (https only). The GIF loads from GIPHY's CDN in the
  // reader's browser. Rating is forced to "g" (work-appropriate). cfg.giphyKey is a
  // public client key; the feature is only shown when it's set.
  function buildGifPicker(ta, cfg, anchor) {
    const panel = el("div", "gc gc-gif"); panel.hidden = true;
    const search = el("input", "gc-gif-search");
    search.type = "search"; search.placeholder = "Search GIPHY…"; search.setAttribute("aria-label", "Search GIPHY");
    const body = el("div", "gc-gif-body");
    const grid = el("div", "gc-gif-grid");
    const overlay = el("div", "gc-gif-overlay");
    const spinner = el("div", "gc-spinner");
    const msg = el("div", "gc-gif-msg");
    overlay.append(spinner, msg);
    body.append(grid, overlay);
    const footer = el("div", "gc-gif-footer");
    const credit = el("a", "gc-gif-credit", "Powered by GIPHY");
    credit.href = "https://giphy.com"; credit.target = "_blank"; credit.rel = "noopener";
    footer.appendChild(credit);
    const closeBtn = el("button", "gc-gif-close"); closeBtn.type = "button";
    closeBtn.title = "Close"; closeBtn.setAttribute("aria-label", "Close");
    closeBtn.innerHTML = octicon("x");
    closeBtn.addEventListener("click", () => close());
    const gifHead = el("div", "gc-gif-head");
    gifHead.append(search, closeBtn);
    panel.append(gifHead, body, footer);

    // The overlay sits on top of the grid: a spinner while loading (old results stay
    // visible, dimmed, so nothing reflows), or a centered message for empty/error.
    function setOverlay(mode, text) {
      if (mode === "off") { overlay.hidden = true; body.classList.remove("gc-loading"); return; }
      spinner.hidden = mode !== "loading";
      msg.hidden = mode !== "message";
      msg.textContent = mode === "message" ? (text || "") : "";
      overlay.hidden = false;
      body.classList.add("gc-loading");
    }

    const API = "https://api.giphy.com/v1/gifs/";
    const PAGE = 24;
    let seq = 0;        // bumped on each new query, to supersede stale fetches
    let curQ = "";      // the active query ("" = trending)
    let offset = 0;     // how many results we've loaded for curQ
    let total = 0;      // total_count GIPHY reports for curQ
    let more = false;   // are there more pages to fetch?
    let busy = false;   // a fetch (first page or next) is in flight

    // A small spinner pinned below the columns; shown while the next page loads and
    // doubles as the IntersectionObserver sentinel for infinite scroll.
    const moreRow = el("div", "gc-gif-more");
    moreRow.appendChild(el("div", "gc-spinner gc-spinner-sm"));
    moreRow.hidden = true;

    // Masonry: GIFs keep their real aspect ratio (variable height), laid out across COLS
    // balanced columns. Each GIF goes to the currently-shortest column, tracked by a
    // running proportional-height tally, so appending a page never reflows what's there.
    const COLS = 2;
    let cols = [];   // column elements
    let colH = [];   // running proportional heights (columns are equal width)
    function resetCols() {
      grid.innerHTML = "";
      cols = []; colH = [];
      const row = el("div", "gc-gif-cols");
      for (let i = 0; i < COLS; i++) {
        const c = el("div", "gc-gif-col");
        row.appendChild(c); cols.push(c); colH.push(0);
      }
      grid.appendChild(row);
      grid.appendChild(moreRow);
    }

    function pageUrl(q, off) {
      return API + (q ? "search" : "trending") +
        "?api_key=" + encodeURIComponent(cfg.giphyKey) +
        "&limit=" + PAGE + "&offset=" + off + "&rating=g&bundle=messaging_non_clips" +
        (q ? "&q=" + encodeURIComponent(q) : "");
    }

    function addItem(g) {
      const im = g.images || {};
      const t = im.fixed_width || im.fixed_width_downsampled || {};
      const thumb = t.url;
      const full = (im.fixed_height || im.downsized_medium || im.original || {}).url;
      if (!thumb || !full) return;
      const w = +t.width || 200, h = +t.height || 200;
      const alt = (g.title || "GIF").replace(/[\[\]()]/g, "").trim().slice(0, 80) || "GIF";
      const item = el("button", "gc-gif-item"); item.type = "button"; item.title = g.title || "GIF";
      const img = el("img"); img.src = thumb; img.alt = alt; img.loading = "lazy";
      img.width = w; img.height = h;                 // reserve correct space, no load jump
      img.style.aspectRatio = w + " / " + h;
      item.appendChild(img);
      item.addEventListener("click", () => {
        insertAtCursor(ta, "![" + alt + "](" + full + ")");
        close();
        ta.focus();
      });
      let k = 0;
      for (let i = 1; i < COLS; i++) if (colH[i] < colH[k]) k = i;
      cols[k].appendChild(item);
      colH[k] += h / w;                              // equal-width columns: h/w ∝ height
    }

    async function load(q) {
      const mine = ++seq;
      curQ = q; offset = 0; total = 0; more = false; busy = true;
      setOverlay("loading");
      let json;
      try {
        const r = await fetch(pageUrl(q, 0));
        if (!r.ok) throw new Error("HTTP " + r.status);
        json = await r.json();
      } catch (e) {
        if (mine === seq) { grid.innerHTML = ""; busy = false; setOverlay("message", "Couldn't reach GIPHY."); }
        return;
      }
      if (mine !== seq) return; // a newer query already superseded this one
      busy = false;
      const data = json.data || [];
      if (!data.length) { grid.innerHTML = ""; setOverlay("message", "No GIFs found."); return; }
      setOverlay("off");
      total = (json.pagination || {}).total_count || 0;
      offset = data.length;
      more = data.length === PAGE && (!total || offset < total);
      resetCols();
      grid.scrollTop = 0; // a new query starts at the top, even if the old one was scrolled
      for (const g of data) addItem(g);
      moreRow.hidden = !more;
    }

    // Fetch the next page and append it (no full overlay, just the bottom spinner) so
    // existing thumbnails stay put. Triggered when the grid is scrolled near its end.
    async function loadMore() {
      if (busy || !more) return;
      const mine = seq;
      busy = true; moreRow.hidden = false;
      let json;
      try {
        const r = await fetch(pageUrl(curQ, offset));
        if (!r.ok) throw new Error("HTTP " + r.status);
        json = await r.json();
      } catch (e) {
        if (mine === seq) { busy = false; more = false; moreRow.hidden = true; }
        return;
      }
      if (mine !== seq) return; // a new search superseded this page
      const data = json.data || [];
      for (const g of data) addItem(g);
      offset += data.length;
      total = (json.pagination || {}).total_count || total;
      more = data.length === PAGE && (!total || offset < total);
      busy = false; moreRow.hidden = !more;
    }

    let timer;
    search.addEventListener("input", () => {
      clearTimeout(timer);
      timer = setTimeout(() => load(search.value.trim()), 300);
    });
    // Robust infinite scroll: observe the bottom sentinel (moreRow) within the grid's
    // own scroll viewport, with a 200px prefetch margin. The scroll listener is a cheap
    // fallback; both funnel through the busy-guarded loadMore().
    grid.addEventListener("scroll", () => {
      if (grid.scrollTop + grid.clientHeight >= grid.scrollHeight - 120) loadMore();
    });
    if ("IntersectionObserver" in window) {
      const io = new IntersectionObserver((entries) => {
        if (entries.some((e) => e.isIntersecting)) loadMore();
      }, { root: grid, rootMargin: "200px 0px" });
      io.observe(moreRow);
    }

    // Floating popup: position:fixed off the GIF button's rect (so it escapes the
    // compose box's overflow:hidden), below the button by default, flipped above when
    // there isn't room. Closes on outside click, Esc, or after a pick.
    function position() {
      // Mobile: the popup goes full-screen (see CSS). Clear the desktop inline
      // positioning so the fixed-inset overlay takes over and nothing drifts as the
      // toolbar reflows.
      if (window.matchMedia("(max-width: 600px)").matches) {
        panel.style.width = ""; panel.style.left = ""; panel.style.top = "";
        return;
      }
      const r = anchor.getBoundingClientRect();
      const W = 320;
      const left = Math.max(8, Math.min(r.right - W, window.innerWidth - W - 8));
      panel.style.width = W + "px";
      panel.style.left = left + "px";
      panel.style.top = (r.bottom + 6) + "px";
      const ph = panel.offsetHeight;
      if (r.bottom + 6 + ph > window.innerHeight - 8 && r.top - ph - 6 > 8) {
        panel.style.top = (r.top - ph - 6) + "px";
      }
    }
    function onDocDown(e) { if (!panel.contains(e.target) && !anchor.contains(e.target)) close(); }
    function onKey(e) { if (e.key === "Escape") { e.stopPropagation(); close(); ta.focus(); } }
    function onReflow() { position(); }
    function open() {
      // Portal the panel to <body> so it escapes any nested stacking context (e.g. the
      // "⋯" overflow dropdown, z-index:60) and its fixed/full-screen overlay sits above
      // the site's fixed header (z-index:10) instead of behind it.
      document.body.appendChild(panel);
      panel.hidden = false;
      position();
      search.value = ""; load(""); search.focus();
      document.addEventListener("mousedown", onDocDown, true);
      document.addEventListener("keydown", onKey, true);
      window.addEventListener("scroll", onReflow, true);
      window.addEventListener("resize", onReflow);
    }
    function close() {
      panel.hidden = true;
      document.removeEventListener("mousedown", onDocDown, true);
      document.removeEventListener("keydown", onKey, true);
      window.removeEventListener("scroll", onReflow, true);
      window.removeEventListener("resize", onReflow);
    }
    panel.gcOpen = open;
    panel.gcClose = close;
    return panel;
  }

  function buildComposer(root, cfg, me, opts) {
    opts = opts || {};
    const replyMode = !!opts.replyToId;
    const editMode = !!opts.editMode;
    const plain = replyMode || editMode; // no avatar; Cancel + action buttons
    const wrap = el("div", "gc-composer" + (editMode ? " gc-composer-edit" : ""));
    const signedIn = !!me.authenticated;

    if (plain) {
      // No avatar in reply/edit mode: the composer is a plain editor inside the
      // bubble, which sidesteps avatar-alignment issues with the nested content.
    } else if (signedIn) {
      const av = el("img", "gc-avatar");
      av.src = me.avatarUrl; av.alt = me.login; wrap.appendChild(av);
    } else {
      // Reserve the avatar slot when signed out so the composer keeps its width.
      const ph = el("div", "gc-avatar gc-avatar-ph");
      ph.setAttribute("aria-hidden", "true");
      ph.innerHTML = octicon("person");
      wrap.appendChild(ph);
    }

    const box = el("div", "gc-compose-box");

    // Write | Preview tabs.
    const tabs = el("div", "gc-tabs");
    const writeTab = el("button", "gc-tab gc-tab-on", "Write"); writeTab.type = "button";
    const previewTab = el("button", "gc-tab", "Preview"); previewTab.type = "button";
    tabs.append(writeTab, previewTab);

    const ta = el("textarea", "gc-textarea");
    ta.placeholder = replyMode ? "Write a reply…"
      : (editMode ? "Edit your comment…"
        : (signedIn ? "Add your comment here..." : "Sign in to comment."));
    if (opts.initialValue) ta.value = opts.initialValue;

    // Auto-grow instead of a manual resize grip: the grip sits at the textarea's corner
    // (mid-widget, above the footer), not at the focus-ring corner, which reads as off.
    // Grow the textarea with its content up to a cap, then let it scroll.
    const TA_MAX = 420;
    function autoGrow() {
      ta.style.height = "auto";
      ta.style.height = Math.min(ta.scrollHeight, TA_MAX) + "px";
    }
    ta.addEventListener("input", autoGrow);
    requestAnimationFrame(autoGrow);

    // Markdown toolbar (Write mode), grouped like GitHub's action-bar, on the
    // same row as the tabs (gc-compose-head). Disabled until the reader signs in.
    // The first few tools stay inline everywhere; the rest live in an overflow group
    // that renders inline on desktop (display:contents) but collapses behind a "⋯"
    // (more) button on mobile, so the toolbar fits on the tabs' row instead of wrapping.
    const toolbar = el("div", "gc-toolbar");

    // Build one formatting tool button (shared by the primary bar and the overflow group).
    function makeTool(icon, before, after, title, mode) {
      const b = el("button", "gc-tool"); b.type = "button"; b.title = title; b.tabIndex = -1;
      b.innerHTML = octicon(icon);
      b.disabled = !signedIn;
      if (signedIn) {
        // Keep focus/selection in the textarea so execCommand inserts undoably.
        b.addEventListener("mousedown", (e) => e.preventDefault());
        b.addEventListener("click", () => {
          if (mode === "line") toggleLinePrefix(ta, before);
          else if (mode === "code") applyCode(ta);
          else if (mode === "link") applyLink(ta);
          else toggleWrap(ta, before, after);
          closeOverflow();
        });
      }
      return b;
    }

    // Primary tools: always inline. Overflow tools (+ the GIF button): inline on desktop,
    // tucked behind the "⋯" on mobile. A `null` entry is a group divider.
    const PRIMARY_TOOLS = [
      ["heading", "### ", "", "Heading", "line"],
      ["bold", "**", "**", "Bold"],
      ["italic", "_", "_", "Italic"],
      ["quote", "> ", "", "Quote", "line"],
      ["code", "`", "`", "Code", "code"],
    ];
    const OVERFLOW_TOOLS = [
      ["link", "[", "](url)", "Link", "link"],
      null,
      ["list-ordered", "1. ", "", "Numbered list", "line"],
      ["list-unordered", "- ", "", "Unordered list", "line"],
      ["tasklist", "- [ ] ", "", "Task list", "line"],
    ];
    for (const t of PRIMARY_TOOLS) toolbar.appendChild(makeTool(...t));

    // The "⋯" more button (shown only on mobile, via CSS) toggles the overflow group.
    const moreBtn = el("button", "gc-tool gc-tool-more"); moreBtn.type = "button";
    moreBtn.title = "More"; moreBtn.tabIndex = -1; moreBtn.setAttribute("aria-label", "More formatting");
    moreBtn.innerHTML = octicon("kebab");
    moreBtn.disabled = !signedIn;
    toolbar.appendChild(moreBtn);

    const overflow = el("div", "gc-tool-overflow");
    for (const t of OVERFLOW_TOOLS) {
      overflow.appendChild(t ? makeTool(...t) : el("span", "gc-tool-divider"));
    }

    // Overflow open/close (mobile dropdown): closes on outside click, Esc, or after a tool.
    let overflowOpen = false;
    function closeOverflow() {
      if (!overflowOpen) return;
      overflowOpen = false; overflow.classList.remove("gc-open");
      document.removeEventListener("mousedown", onOverflowDown, true);
      document.removeEventListener("keydown", onOverflowKey, true);
    }
    function onOverflowDown(e) { if (!overflow.contains(e.target) && !moreBtn.contains(e.target)) closeOverflow(); }
    function onOverflowKey(e) { if (e.key === "Escape") { e.stopPropagation(); closeOverflow(); ta.focus(); } }
    if (signedIn) {
      moreBtn.addEventListener("mousedown", (e) => e.preventDefault());
      moreBtn.addEventListener("click", () => {
        if (overflowOpen) { closeOverflow(); return; }
        overflowOpen = true; overflow.classList.add("gc-open");
        document.addEventListener("mousedown", onOverflowDown, true);
        document.addEventListener("keydown", onOverflowKey, true);
      });
    }
    toolbar.appendChild(overflow);

    // Markdown-supported hint (a markdown-icon link). Placed in the actions footer,
    // beside the "Signed in as…" / "Sign in…" text (built below), not in the toolbar.
    const mdHint = el("a", "gc-md-hint");
    mdHint.href = "https://docs.github.com/get-started/writing-on-github";
    mdHint.target = "_blank"; mdHint.rel = "noopener"; mdHint.title = "Markdown is supported";
    mdHint.innerHTML = octicon("markdown");

    // Optional GIPHY picker (client-side; only when a key is configured). The button lives
    // in the overflow group (so it tucks into the "⋯" on mobile). Its popup floats off the
    // button on desktop and goes full-screen on mobile (see buildGifPicker/position).
    let gifPanel = null;
    if (cfg.giphyKey) {
      const gifWrap = el("span", "gc-gif-wrap");
      const gifBtn = el("button", "gc-tool gc-tool-gif", "GIF");
      gifBtn.type = "button"; gifBtn.title = "Add a GIF"; gifBtn.tabIndex = -1;
      // Shown whenever GIPHY is configured, disabled until signed in (like the other tools).
      gifBtn.disabled = !signedIn;
      gifWrap.appendChild(gifBtn);
      if (signedIn) {
        gifPanel = buildGifPicker(ta, cfg, gifBtn);
        gifWrap.appendChild(gifPanel);
        gifBtn.addEventListener("mousedown", (e) => e.preventDefault());
        gifBtn.addEventListener("click", () => {
          if (gifPanel.hidden) gifPanel.gcOpen(); else gifPanel.gcClose();
        });
      }
      // A divider separates the GIF button from the list group before it.
      overflow.appendChild(el("span", "gc-tool-divider"));
      overflow.appendChild(gifWrap);
    }

    // Preview pane (GitHub-rendered, reuses .gc-body so it matches a posted comment).
    const preview = el("div", "gc-preview gc-body");
    preview.hidden = true;

    let lastText = null, lastHtml = "";     // browser-side preview cache
    let pfInflight = null, pfText = null, pfSeq = 0;  // in-flight request (dedup + staleness)
    let lastPrefetchAt = 0;

    // Render `text` to preview HTML via the backend, caching the result and de-duplicating
    // concurrent requests for the same text. Shared by the Preview tab and the on-intent
    // prefetch below, so a hover-warmed cache makes clicking Preview instant. A monotonic
    // seq guards against a slow older response clobbering a newer one.
    function fetchPreview(text) {
      if (text === lastText) return Promise.resolve(lastHtml);   // cache hit
      if (pfInflight && pfText === text) return pfInflight;       // already fetching this text
      const seq = ++pfSeq;
      pfText = text;
      const p = (async () => {
        const r = await api(cfg, "/api/preview", {
          method: "POST", headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ text: text }),
        });
        if (!r.ok) throw new Error("HTTP " + r.status);
        const d = await r.json();
        const html = (d.html && d.html.trim()) ? d.html : '<p class="gc-muted">Nothing to preview.</p>';
        if (seq === pfSeq) { lastText = text; lastHtml = html; }  // only cache the latest request
        return html;
      })();
      pfInflight = p;
      p.then(() => {}, () => {}).then(() => { if (pfInflight === p) { pfInflight = null; pfText = null; } });
      return p;
    }

    // Warm the cache on *intent* to preview (hovering/focusing the Preview tab), throttled, so
    // the round-trip finishes before the click, without prefetching for the many users who
    // just type and submit. Skips when empty or already cached; fetchPreview de-dupes the rest.
    function prefetchPreview() {
      const text = ta.value;
      if (!text.trim() || text === lastText) return;
      const now = Date.now();
      if (now - lastPrefetchAt < 500) return;   // throttle repeated intent
      lastPrefetchAt = now;
      fetchPreview(text).catch(() => {});        // ignore errors here; showPreview surfaces them
    }

    function showWrite() {
      writeTab.classList.add("gc-tab-on"); previewTab.classList.remove("gc-tab-on");
      toolbar.hidden = false; preview.hidden = true; ta.hidden = false; ta.focus();
    }
    async function showPreview() {
      previewTab.classList.add("gc-tab-on"); writeTab.classList.remove("gc-tab-on");
      // Hold the editor's current (possibly user-resized) height so nothing jumps.
      preview.style.minHeight = ta.offsetHeight + "px";
      toolbar.hidden = true; ta.hidden = true; preview.hidden = false;
      const text = ta.value;
      if (!text.trim()) { preview.innerHTML = '<p class="gc-muted">Nothing to preview.</p>'; return; }
      if (text === lastText) { preview.innerHTML = lastHtml; return; }  // instant (often hover-warmed)
      preview.innerHTML = '<p class="gc-muted">Loading preview…</p>';
      try {
        preview.innerHTML = await fetchPreview(text);
      } catch (e) {
        preview.innerHTML = '<p class="gc-muted">Preview failed (' + e.message + ").</p>";
      }
    }

    const actions = el("div", "gc-compose-actions");

    let submit;
    if (signedIn) {
      // Enabled composer: tabs switch, toolbar formats, submit posts.
      writeTab.addEventListener("click", showWrite);
      previewTab.addEventListener("click", showPreview);
      // Prefetch on intent to open Preview (hover / keyboard focus / touch), throttled, so the
      // round-trip is already done by click time, only for users who actually reach for it.
      previewTab.addEventListener("pointerenter", prefetchPreview);
      previewTab.addEventListener("focus", prefetchPreview);
      previewTab.addEventListener("touchstart", prefetchPreview, { passive: true });
      submit = el("button", "gc-submit", editMode ? "Update comment" : (replyMode ? "Reply" : "Comment"));
      const onSubmit = submitter(root, cfg, opts);
      submit.addEventListener("click", () => onSubmit(ta, submit));
      // Keyboard shortcuts: Cmd/Ctrl+Enter submits; +B/+I/+K format.
      ta.addEventListener("keydown", (e) => {
        const mod = e.metaKey || e.ctrlKey;
        if (!mod) return;
        if (e.key === "Enter") { e.preventDefault(); onSubmit(ta, submit); return; }
        if (e.altKey || e.shiftKey) return;
        const k = e.key.toLowerCase();
        if (k === "b") { e.preventDefault(); toggleWrap(ta, "**", "**"); }
        else if (k === "i") { e.preventDefault(); toggleWrap(ta, "_", "_"); }
        else if (k === "k") { e.preventDefault(); applyLink(ta); }
      });

      // Smart Markdown Enter: continue a list / quote / task item on Enter (ordered lists
      // auto-increment, task items continue unchecked); a second Enter on an empty item
      // clears its marker and exits the list. execCommand keeps the native undo stack.
      function listContinuation(line) {
        let m;
        if ((m = /^(\s*[-*+]\s+\[[ xX]\]\s+)(.*)$/.exec(line)))     // - [ ] task
          return { next: m[1].replace(/\[[ xX]\]/, "[ ]"), empty: m[2] === "" };
        if ((m = /^(\s*[-*+]\s+)(.*)$/.exec(line)))                 // - item
          return { next: m[1], empty: m[2] === "" };
        if ((m = /^(\s*)(\d+)([.)])(\s+)(.*)$/.exec(line)))         // 1. item
          return { next: m[1] + (parseInt(m[2], 10) + 1) + m[3] + m[4], empty: m[5] === "" };
        if ((m = /^(\s*>+ ?)(.*)$/.exec(line)))                     // > quote
          return { next: m[1], empty: m[2] === "" };
        return null;
      }
      ta.addEventListener("keydown", (e) => {
        if (e.key !== "Enter" || e.shiftKey || e.metaKey || e.ctrlKey || e.altKey) return;
        const val = ta.value, s = ta.selectionStart;
        if (s !== ta.selectionEnd) return;
        const ls = val.lastIndexOf("\n", s - 1) + 1;
        let le = val.indexOf("\n", s); if (le === -1) le = val.length;
        const cont = listContinuation(val.slice(ls, le));
        if (!cont) return;
        e.preventDefault();
        if (cont.empty) { ta.setSelectionRange(ls, le); document.execCommand("delete"); }
        else document.execCommand("insertText", false, "\n" + cont.next);
        autoGrow();
      });

      // Paste a bare URL over a selection → a Markdown link: [selection](url).
      ta.addEventListener("paste", (e) => {
        const s = ta.selectionStart, en = ta.selectionEnd;
        if (s === en) return;
        const url = ((e.clipboardData && e.clipboardData.getData("text")) || "").trim();
        if (!url || /\s/.test(url) || !/^(https?:\/\/|www\.)\S+$/i.test(url)) return;
        e.preventDefault();
        document.execCommand("insertText", false, "[" + ta.value.slice(s, en) + "](" + url + ")");
        autoGrow();
      });
      if (plain) {
        // Cancel discards the editor: confirm first if there are unsaved changes
        // (content differs from where the editor started: empty for a new reply, the
        // original body for an edit).
        const doCancel = () => {
          const initial = opts.initialValue || "";
          if (ta.value.trim() !== initial.trim() &&
              !confirm("Are you sure you want to discard your unsaved changes?")) return;
          if (opts.onCancel) opts.onCancel();
        };
        const cancel = el("button", "gc-cancel", "Cancel"); cancel.type = "button";
        cancel.addEventListener("click", doCancel);
        // Esc in the textarea is the same as clicking Cancel.
        ta.addEventListener("keydown", (e) => {
          if (e.key === "Escape") { e.preventDefault(); doCancel(); }
        });
        actions.append(cancel, submit);
      } else {
        const hint = el("span", "gc-hint");
        const meName = me.name && me.name !== me.login ? me.name : displayHandle(me.login);
        hint.textContent = "Signed in as " + meName;
        const out = el("a", null, "Sign out"); out.href = "#";
        out.addEventListener("click", async (e) => {
          e.preventDefault();
          gcToken(cfg, null);
          await api(cfg, "/auth/logout", { method: "POST" }).catch(function () {});
          refresh(root, cfg);
        });
        hint.appendChild(document.createTextNode(" · "));
        hint.appendChild(out);
        const leftIn = el("span", "gc-actions-left");
        leftIn.append(mdHint, hint);
        actions.append(leftIn, submit);
      }
    } else {
      // Signed out: the whole composer is shown but disabled, and the action
      // button becomes the tenant provider's sign-in action (the only thing the reader can do).
      ta.disabled = true;
      writeTab.disabled = true; previewTab.disabled = true;
      const hint = el("span", "gc-hint");
      const provider = me.oauthName || "OAuth";
      hint.textContent = "Sign in with " + provider + " to comment.";
      submit = el("button", "gc-signin-btn"); submit.type = "button";
      if (provider === "GitHub") submit.innerHTML = GITHUB_MARK_SVG;
      submit.appendChild(el("span", null, "Sign in with " + provider));
      submit.addEventListener("click", async () => {
        submit.disabled = true;
        try { await openLogin(cfg); }
        catch (_) { submit.disabled = false; alert("Could not start sign-in."); }
      });
      const leftOut = el("span", "gc-actions-left");
      leftOut.append(mdHint, hint);
      actions.append(leftOut, submit);
    }

    // Tabs + toolbar share one row; the toolbar is hidden in Preview (above).
    const head = el("div", "gc-compose-head");
    head.append(tabs, toolbar);
    // Wrap the editor + preview so both get the same padding inside the box and
    // don't shift when switching Write/Preview.
    const bodyWrap = el("div", "gc-compose-body");
    bodyWrap.append(ta, preview);
    box.append(head, bodyWrap, actions);
    wrap.appendChild(box);
    return wrap;
  }

  async function refresh(root, cfg) {
    const me = await fetchMe(cfg);
    SIGNED_IN = !!me.authenticated;
    ME = me;
    const next = buildComposer(root, cfg, me);
    // Only the main composer is a direct child of root; reply composers live inside
    // the list, so scope the lookup so we never replace a nested reply composer.
    const old = root.querySelector(":scope > .gc-composer");
    if (old) old.replaceWith(next);
    else root.appendChild(next);
  }

  async function mount(root) {
    const cfg = {
      backend: root.dataset.backend || window.GC_BACKEND || "",
      // Optional static capability for a protected tenant. Empty means public.
      accessKey: root.dataset.accessKey || window.GC_ACCESS_KEY || "",
      // A stable per-page key (the post slug); the backend stores this page's
      // comments under it.
      term: root.dataset.term || "",
      title: root.dataset.title || "",
      subtitle: root.dataset.subtitle || "",
      url: root.dataset.url || "",
      // Optional login suffix to drop when *displaying* handles (e.g. "_Acme").
      // The full login stays the identity key; this is cosmetic only.
      stripSuffix: root.dataset.stripSuffix || "",
      // Public GIPHY client key; when set, the composer shows a GIF picker.
      giphyKey: root.dataset.giphyKey || "",
    };
    CFG = cfg;
    ROOT = root;
    absorbAuthToken(cfg);
    root.innerHTML =
      '<div class="gc-header"><span class="gc-count">Comments</span></div>' +
      '<div class="gc-list"></div>';
    // Fill any per-site config the page didn't supply from the backend (resolved by
    // Origin), so the same widget works for any registered tenant. Skipped entirely when
    // the page already provides everything (the common case), so there's no extra fetch.
    if (cfg.backend && (!cfg.giphyKey || !cfg.stripSuffix)) await fetchConfig(cfg);
    // Resolve auth before rendering comments so reaction controls paint in the
    // correct (signed-in vs signed-out) state on first load.
    await refresh(root, cfg);
    if (cfg.term) load(root, cfg);
    window.addEventListener("message", function (e) {
      if (e.data === "gc-auth-done") {
        // Re-resolve auth, then re-render the list so reactions become interactive.
        refresh(root, cfg).then(function () { if (cfg.term) load(root, cfg); });
      }
    });
    // Mobile fallback: the OAuth flow often opens in a new tab where window.opener (and so
    // the postMessage above) does not bridge back. When the reader returns to this tab, the
    // session cookie is already set, so re-resolve auth on the way back in. Guarded on
    // SIGNED_IN so a plain tab switch while signed in costs nothing.
    document.addEventListener("visibilitychange", function () {
      if (document.visibilityState === "visible" && !SIGNED_IN) {
        refresh(root, cfg).then(function () { if (cfg.term) load(root, cfg); });
      }
    });
    // Re-focus when the hash changes to a comment on the current page (e.g. the
    // reader pasted a "Copy link" URL while already here).
    window.addEventListener("hashchange", function () { focusHashComment(root); });
  }

  function init() {
    document.querySelectorAll(".gc[data-term]").forEach(mount);
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
