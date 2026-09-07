/* Purpose: entry point and super-loop for the decision module.
 * Owning module: decision. */

/* Stress-matrix navigation -> next (F, A) region.
 *
 * PLACEHOLDER. The structure is fixed, the implementation is not written yet;
 * the module owner writes it once the design document is agreed.
 *
 * This file does init plus the super-loop and nothing else, aiming for under 50
 * lines. Everything it calls lives in src/hal/, src/app/ or src/ui/:
 *
 *     init hardware        (src/hal/)
 *     init display         (src/ui/)
 *     loop forever:
 *         read inputs      (src/hal/)
 *         compute          (src/app/, the part you can test on a laptop)
 *         drive outputs    (src/hal/)
 *         update display   (src/ui/)
 *
 * Vendor headers are allowed here and in src/hal/, never under src/app/.
 * See docs/coding-standard.md §1.
 *
 * TODO(dec): implement after the design document is agreed.
 * TODO(mgmt): backbone framing is undecided, see open questions 1, 2, 4 and 5
 *             in docs/interface-contract.md.
 */
