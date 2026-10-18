# Security

PfeilZaehler3000 is a hobby project. If you find a security problem – for example a way to change
or read someone else's pod over Bluetooth despite the owner lock, or a way for the phone page to
send data somewhere – please don't open a public issue.

Report it privately instead: on the repository page, **Security → Report a vulnerability**. Describe
what you found and how to reproduce it. You'll get an answer as soon as possible; there is no bounty.

**What the project does:**
- The phone page runs entirely in the browser. It stores the arrows in that browser and sends
  nothing to any server.
- The pod is a Bluetooth Low Energy device without pairing. With the owner lock (pod firmware 2.3),
  only devices that have become owners while the pod was on USB power may change it or download its
  arrows; the counts (today, week, month, battery) stay readable for everyone nearby.
- The watch app talks only to its own pod.

Only the latest release is maintained.
