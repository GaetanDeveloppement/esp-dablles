<script>
    import { api } from "./api.js";
    import { onMount } from "svelte";

    let status = {
        state: "idle",
        position: 0,
        speed: 5,
        steps_per_rev: 2048,
    };

    let steps = 512;
    let speed = 5;
    let loading = false;

    // Poll status every 500ms
    onMount(() => {
        const interval = setInterval(async () => {
            try {
                status = await api.getStatus();
            } catch (error) {
                console.error("Failed to fetch status:", error);
            }
        }, 500);

        return () => clearInterval(interval);
    });

    async function rotate(direction) {
        loading = true;
        try {
            await api.rotate(direction, steps, speed);
        } catch (error) {
            console.error("Rotation failed:", error);
        }
        loading = false;
    }

    async function stop() {
        try {
            await api.stop();
        } catch (error) {
            console.error("Stop failed:", error);
        }
    }

    async function reset() {
        try {
            await api.reset();
            status = await api.getStatus();
        } catch (error) {
            console.error("Reset failed:", error);
        }
    }
</script>

<div class="motor-control">
    <h1>🔄 Stepper Motor Control</h1>

    <!-- Status Display -->
    <div class="status-card">
        <h2>Motor Status</h2>
        <div class="status-grid">
            <div class="status-item">
                <span class="label">State:</span>
                <span class="value {status.state}">{status.state}</span>
            </div>
            <div class="status-item">
                <span class="label">Position:</span>
                <span class="value">{status.position} steps</span>
            </div>
            <div class="status-item">
                <span class="label">Speed:</span>
                <span class="value">{status.speed}ms</span>
            </div>
        </div>
    </div>

    <!-- Controls -->
    <div class="controls-card">
        <h2>Controls</h2>

        <div class="input-group">
            <label for="steps">Steps:</label>
            <input
                id="steps"
                type="number"
                bind:value={steps}
                min="1"
                max="10000"
            />
        </div>

        <div class="input-group">
            <label for="speed">Speed (ms):</label>
            <input
                id="speed"
                type="range"
                bind:value={speed}
                min="1"
                max="20"
            />
            <span>{speed}ms</span>
        </div>

        <div class="button-group">
            <button
                class="btn btn-cw"
                on:click={() => rotate(1)}
                disabled={loading || status.state === "rotating"}
            >
                ↻ Clockwise
            </button>

            <button
                class="btn btn-ccw"
                on:click={() => rotate(0)}
                disabled={loading || status.state === "rotating"}
            >
                ↺ Counter-CW
            </button>
        </div>

        <div class="button-group">
            <button
                class="btn btn-full-cw"
                on:click={() => {
                    steps = 2048;
                    rotate(1);
                }}
                disabled={loading || status.state === "rotating"}
            >
                Full Rotation CW
            </button>

            <button
                class="btn btn-full-ccw"
                on:click={() => {
                    steps = 2048;
                    rotate(0);
                }}
                disabled={loading || status.state === "rotating"}
            >
                Full Rotation CCW
            </button>
        </div>

        <div class="button-group">
            <button class="btn btn-stop" on:click={stop}> ⏹ Stop </button>

            <button class="btn btn-reset" on:click={reset}>
                🔄 Reset Position
            </button>
        </div>
    </div>
</div>

<style>
    .motor-control {
        max-width: 600px;
        margin: 50px auto;
        font-family:
            -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen,
            Ubuntu, Cantarell, sans-serif;
    }

    h1 {
        text-align: center;
        color: #333;
        margin-bottom: 30px;
    }

    .status-card,
    .controls-card {
        background: white;
        border-radius: 12px;
        padding: 24px;
        margin-bottom: 20px;
        box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
    }

    h2 {
        margin-top: 0;
        font-size: 18px;
        color: #666;
    }

    .status-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
        gap: 16px;
    }

    .status-item {
        display: flex;
        flex-direction: column;
        gap: 4px;
    }

    .label {
        font-size: 12px;
        color: #888;
        text-transform: uppercase;
    }

    .value {
        font-size: 20px;
        font-weight: 600;
        color: #333;
    }

    .value.rotating {
        color: #f59e0b;
    }

    .value.idle {
        color: #10b981;
    }

    .input-group {
        margin-bottom: 20px;
        display: flex;
        align-items: center;
        gap: 12px;
    }

    .input-group label {
        font-weight: 500;
        min-width: 100px;
    }

    input[type="number"] {
        padding: 8px 12px;
        border: 2px solid #e5e7eb;
        border-radius: 6px;
        font-size: 16px;
        width: 120px;
    }

    input[type="range"] {
        flex: 1;
    }

    .button-group {
        display: flex;
        gap: 12px;
        margin-bottom: 12px;
    }

    .btn {
        flex: 1;
        padding: 14px 24px;
        font-size: 16px;
        font-weight: 600;
        border: none;
        border-radius: 8px;
        cursor: pointer;
        color: white;
        transition: all 0.2s;
    }

    .btn:hover:not(:disabled) {
        transform: translateY(-2px);
        box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
    }

    .btn:disabled {
        opacity: 0.5;
        cursor: not-allowed;
    }

    .btn-cw {
        background: #10b981;
    }

    .btn-ccw {
        background: #3b82f6;
    }

    .btn-full-cw {
        background: #059669;
    }

    .btn-full-ccw {
        background: #2563eb;
    }

    .btn-stop {
        background: #ef4444;
    }

    .btn-reset {
        background: #6b7280;
    }
</style>
