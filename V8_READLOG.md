# v8 Read Log — "where the numbers come from"

Every previous version treated randomness as a solved problem: `std::mt19937`
wrapped in a class, `std::exponential_distribution` for the draws, done. v8 opens
that box, because two things live inside it that a simulation course cares about
and that this engine could not do.

Status: **260/260 checks** (215 in v7), clean under `-Wall -Wextra -Wpedantic`
and ASan/UBSan.

---

## 1. One primitive, and everything built on it

```cpp
double RandomStream::u01();      // uniform on the OPEN interval (0,1)
```

Every variate is now that uniform pushed through an inverse CDF. Exponential is
`-mean * ln(1-u)`. Weibull is `scale * (-ln(1-u))^(1/shape)`. Normal is
Acklam's rational approximation to the normal quantile.

**This is not tidiness.** Two techniques depend on it and neither works without:

- **Common random numbers** needs streams that line up between model variants.
- **Antithetic variates** needs *exactly one* uniform per variate, transformed
  *monotonically*, or "the opposite draw" has no meaning.

`std::exponential_distribution` is a fine generator and completely opaque — you
cannot know how many uniforms it consumes, so you cannot pair anything with
anything. That is the whole reason it had to go.

Two details that are easy to get wrong:

**The interval is open.** `u01()` never returns exactly 0 or 1, because inverse
transforms take `log(u)` and `log(1-u)` and either endpoint is an infinity.

**`ln(1-u)`, not `ln(u)`.** Both are correct *in distribution*. Only the first is
monotonically increasing in u, which is what antithetic pairing relies on.

**Normal uses inverse transform, not Box-Muller.** Box-Muller is faster and
consumes two uniforms to make two variates, neither a monotone function of
either — so antithetic runs would not be antithetic. One uniform in, one variate
out, monotone, is worth more here than speed.

---

## 2. Testing the generator — and RANDU

A broken generator does not crash. It produces plausible numbers, runs your
model, and hands you an answer. `StreamTests` implements the standard checks:
chi-square uniformity, Kolmogorov–Smirnov, runs up-and-down, autocorrelation at
lag k, and a **serial test in three dimensions**.

Then `EngineKind::Randu` — IBM's generator, shipped for years — to show why the
last one is there:

```
--- RANDU (1960s IBM) -------------------------------
  chi-square uniformity        8.724  vs   16.919   pass
  Kolmogorov-Smirnov           0.916  vs    1.358   pass
  runs up and down             0.673  vs    1.960   pass
  autocorrelation at lag 1     0.622  vs    1.960   pass
  autocorrelation at lag 5     1.213  vs    1.960   pass
  serial test in 3D        79994.573  vs 4244.989   FAIL   257 of 4096 cells never visited
```

**It passes every one-dimensional test and fails the three-dimensional one by a
factor approaching twenty.** RANDU satisfies `x[n+2] = 6x[n+1] - 9x[n] (mod 2^31)`
exactly, so every consecutive triple lies on one of 15 planes in the unit cube.
Invisible in one dimension; a lattice in three.

Simulations consume several numbers per event constantly — an interarrival, a
service time, a branch draw — so three dimensions is not an exotic stress test.

**Resolution turned out to be the whole test.** At 8 bins per axis RANDU's planes
still land in most cells and it scrapes a pass (552 vs 565). At 16 bins the
statistic is 15× the critical value; at 20, it is 29× with a quarter of the cells
empty. The default is now 16.

Also worth saying in the report: **a pass is not proof.** It means this test
found nothing. A fail is much stronger evidence — a good generator fails a 95%
test only 1 time in 20, which also means an occasional isolated failure from
mt19937 is expected.

---

## 3. Seven more distributions

`Normal`, `Lognormal`, `Weibull`, `Erlang`, `Discrete`, `Empirical`, `Poisson` —
each with a `mean()` so the stability check still works, and each documented with
*when to reach for it* rather than only what it is.

Three that carry a real trap:

**Normal used as a duration has a left tail**, and a negative service time is not
a subtlety, it is nonsense. There are two honest answers — clamp at zero
(changing the distribution slightly) or refuse — and the class does whichever you
ask, with truncation the default. Silently returning a negative is not on the
list.

**Lognormal's parameters are the mean and sd of the LOGARITHM.** That is the
convention and a famous way to be wrong by a factor of several, so
`lognormalFrom(mean, sd)` does the conversion — `sigma² = ln(1 + (sd/mean)²)`,
`mu = ln(mean) - sigma²/2` — in one place.

**`Empirical::mean()` is not the average of the observations.** `draw()`
interpolates linearly between order statistics, so what it actually samples has
the *trapezoidal* mean, weighting interior points twice and the extremes once.
For a skewed sample the two differ visibly — 4.786 against 4.796 in example 13 —
and since this number feeds the stability check it has to be the mean of what is
drawn, not of what was measured. That one was caught by the example printing both
side by side.

---

## 4. Streams, and what they buy

`RandomStream::substream(name)` derives an independent stream from the base seed
and a name (FNV-1a, so it is deterministic). `useSeparateStreams()` gives each
distribution its own, named after its role — `"arrivals"`, `"service:Teller"`.

The point is that **changing the service distribution no longer shifts the
arrival pattern**, so replication *i* of one variant and replication *i* of
another see the same arrivals.

### Antithetic variates

Each replication runs twice — once normally, once with every uniform mirrored —
and the pair is averaged into **one** result. Same 40 runs of compute:

```
40 independent replications   3.1870  +/-  0.1361
20 antithetic pairs           3.1617  +/-  0.1040
```

24% narrower for nothing. And the pair is one observation, not two: counting the
halves as independent would understate the interval, which is the one direction
you must never err in.

### Common random numbers

The result that makes the case, comparing one fast server against two slow ones:

```
design A, on its own    3.2866  +/-  0.2862   [3.0004, 3.5728]
design B, on its own    2.9167  +/-  0.2791   [2.6376, 3.1958]
intervals overlap: true      -> cannot say which is better

paired difference (A-B) 0.3699  +/-  0.0160   [0.3539, 0.3859]   significant
an UNPAIRED comparison would give +/- 0.3998  (25x wider)
```

**Twenty-five times tighter, from the same runs.** The shared bad luck inflates
both designs together, so it cancels in A−B.

The lesson worth carrying: *two designs whose intervals overlap have not been
shown to differ — but they may still differ.* Overlapping intervals are weak
evidence, not evidence of no difference. Whenever you are **comparing** rather
than **measuring**, pair.

---

## What is deliberately incomplete

**Only three roles get their own stream** — arrivals, arrival attributes, and
each Process's service time. Delay durations and Decide draws still share the
common stream, so variants that differ in *those* pair less well. Stated in the
code and in example 14 rather than hidden.

**No general Gamma.** A non-integer shape needs an acceptance–rejection method,
which consumes an unpredictable number of uniforms per variate — destroying the
one-uniform-per-variate property that everything above depends on. Erlang covers
the integer-shape case. Adding Gamma means deciding what to do about that, and it
is a bigger decision than a distribution.

**Poisson uses inverse transform**, which walks the cumulative mass. Fine for the
small means models use; for a large mean the loop gets long and rejection would
be better, at the same cost. Noted, not fixed.

---

## A note on reproducibility across versions

The generators changed, so **random-driven examples print different numbers than
they did in v7.** Example 01's utilisation moved from 0.7443 to 0.8432 on a
single 480-minute run. Nothing is wrong: the model, the theory and the intervals
are unchanged, and the deterministic examples (02's hand-worked table: 5 served,
average wait exactly 1.0000, last exit t=13) are bit-identical.

This is the one version where "every example reproduces its previous numbers"
could not be the acceptance criterion, and it is worth being explicit about
rather than quietly letting the numbers drift.

---

## Verified

- 260/260 checks; the 45 new ones cover the open interval, antithetic mirroring,
  substream determinism and independence, all seven distributions against their
  theoretical means, Normal truncation and refusal, the trapezoidal `Empirical`
  mean, the chi-square table, mt19937 passing everything, RANDU passing all the
  1-D tests and failing the 3-D one by a wide margin, and both variance-reduction
  techniques actually reducing variance.
- Clean under ASan/UBSan.
