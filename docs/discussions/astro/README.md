# Astro integration

Import the packaged component directly from `discussions/astro/Discussions.astro`:

```astro
---
import Discussions from '../discussions/astro/Discussions.astro';
---

<Discussions
  backend="https://comments.example.com/my-blog"
  term="blog/my-post"
  title="My post"
  url="https://blog.example.com/blog/my-post/"
/>
```

`term` must be stable for the lifetime of the page. The backend scopes it to the tenant,
so different tenants can safely use the same term.

The component also accepts build-time defaults:

```dotenv
PUBLIC_DISCUSSIONS_BACKEND=https://comments.example.com/my-blog
PUBLIC_DISCUSSIONS_ACCESS_KEY=
```

Pass a non-empty `accessKey` only for a capability-protected tenant. Because Astro embeds
it in the generated page, every reader who can load that page can inspect it. It prevents
anonymous discovery and direct API access, but it is not per-reader authentication.

See [`../widget/README.md`](../widget/README.md) for every prop and data attribute.
