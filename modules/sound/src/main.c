/* Purpose: entry point and super-loop for the sound module.
 * Owning module: sound. */

/* Microphone conditioning -> loudness + trend.
 *
 * PLACEHOLDER — no implementation yet, by decision of the repository owner.
 * This file is scaffolding: the structure is fixed, the code is not written.
 * The module owner writes it once the design document is agreed.
 *
 * When you do:
 *
 *   - Init plus super-loop ONLY. Target <= 50 lines.
 *   - Everything this file calls lives in src/hal/, src/app/ or src/ui/.
 *     If logic starts accumulating here, it belongs in src/app/.
 *   - Vendor headers (xparameters.h, xil_*.h, ...) are allowed here and in
 *     src/hal/. They are NOT allowed under src/app/ — see
 *     docs/coding-standard.md §1.
 *   - Shape to aim for:
 *
 *         init hardware        (src/hal/)
 *         init display         (src/ui/)
 *         loop forever:
 *             read inputs      (src/hal/)
 *             compute          (src/app/  <- the part you can test on a laptop)
 *             drive outputs    (src/hal/)
 *             update display   (src/ui/)
 *
 * TODO(snd): implement after the design document is agreed.
 * TODO(mgmt):    backbone framing is still undecided — see
 *                docs/interface-contract.md, open questions 1, 2, 4 and 5.
 */
