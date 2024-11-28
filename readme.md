## Code Sample: Triggering interruptions between the Media Engine and Main CPU

This code sample demonstrates how to trigger an interrupt on the Media Engine (ME) from the main CPU and vice versa. To achieve this, it is necessary to enable system-level interrupts for the Media Engine and external interrupts at its CP0 level, via the IM mask above the Status register.

An exception handler must be configured. In this example, this is done by registering a virtual address above the `ebase`. This handler simply redirects execution to a safer location, outside the exception vector, allowing us to cancel system interrupts and increment a proof-of-work counter in the eDRAM.

**Note**: The handler does not account for delay slots, but handling these would likely be necessary in more complex senarios.

The code prioritizes the use of assembly to provide better visibility into the affected registers. As such, we use `k0` and `k1` for any manipulation outside the program's normal execution flow. Despite this, it is essential to save and restore the kernel register context to ensure system stability.

Additional comments are included in the code to aid comprehension.

On the main CPU side, the process is much simpler. We use SCE functions to activate and register the handler.


## Usage
1. Compile the code using `./build.sh`.
2. Run the binary on a PSP (not on an emulator).
3. Press **Triangle** to trigger an interrupt on the Media Engine and retrieve proof-of-work.
4. Press **Square** to do the same from the main CPU.

## Special Thanks To
- Contributors to psdevwiki.
- The PSP homebrew community, for being an invaluable source of knowledge.
- *crazyc* from ps2dev.org, for pioneering discoveries related to the Media Engine.
- All developers and contributors who have supported and continue to support the scene.

# resources:
- [uofw on GitHub](https://github.com/uofw/uofw)
- [psp wiki on PSDevWiki](https://www.psdevwiki.com/psp/)
- [pspdev on GitHub](https://github.com/pspdev)

*m-c/d*
