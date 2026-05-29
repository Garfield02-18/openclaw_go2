import { logger } from "../util/logger.js";
import { toggleDebugMode } from "./debug-mode.js";
import { sendMessageWeixin } from "./send.js";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
/** 发送回复消息 */
async function sendReply(ctx, text) {
    const opts = {
        baseUrl: ctx.baseUrl,
        token: ctx.token,
        contextToken: ctx.contextToken,
    };
    await sendMessageWeixin({ to: ctx.to, text, opts });
}
const execFileAsync = promisify(execFile);
const GO2_BRIDGE_SCRIPT = "/home/radxa/go2_bridge_ros2/scripts/wechat_go2.sh";
async function handleGo2(ctx, args) {
    const trimmed = args.trim();
    if (!trimmed) {
        await sendReply(ctx, `用法:
/go2 status
/go2 stand-up
/go2 stop
/go2 sit
/go2 rise-sit
/go2 recover-stand
/go2 move --vx 0.2 --vy 0 --vyaw 0 --duration 1.0`);
        return;
    }
    const argv = trimmed.split(/\s+/).filter(Boolean);
    try {
        const { stdout, stderr } = await execFileAsync(GO2_BRIDGE_SCRIPT, argv, { env: process.env, timeout: 30000, maxBuffer: 1024 * 1024 });
        const text = [stdout, stderr].filter(Boolean).join('\n').trim();
        await sendReply(ctx, text || 'go2 command completed');
    } catch (err) {
        const stdout = err && typeof err === 'object' && 'stdout' in err ? String(err.stdout || '') : '';
        const stderr = err && typeof err === 'object' && 'stderr' in err ? String(err.stderr || '') : '';
        const message = err instanceof Error ? err.message : String(err);
        const text = [stdout, stderr, message].filter(Boolean).join('\n').trim();
        await sendReply(ctx, text || 'go2 command failed');
    }
}

/** 处理 /echo 指令 */
async function handleEcho(ctx, args, receivedAt, eventTimestamp) {
    const message = args.trim();
    if (message) {
        await sendReply(ctx, message);
    }
    const eventTs = eventTimestamp ?? 0;
    const platformDelay = eventTs > 0 ? `${receivedAt - eventTs}ms` : "N/A";
    const timing = [
        "⏱ 通道耗时",
        `├ 事件时间: ${eventTs > 0 ? new Date(eventTs).toISOString() : "N/A"}`,
        `├ 平台→插件: ${platformDelay}`,
        `└ 插件处理: ${Date.now() - receivedAt}ms`,
    ].join("\n");
    await sendReply(ctx, timing);
}
/**
 * 尝试处理斜杠指令
 *
 * @returns handled=true 表示该消息已作为指令处理，不需要继续走 AI 管道
 */
export async function handleSlashCommand(content, ctx, receivedAt, eventTimestamp) {
    const trimmed = content.trim();
    if (!trimmed.startsWith("/")) {
        return { handled: false };
    }
    const spaceIdx = trimmed.indexOf(" ");
    const command = spaceIdx === -1 ? trimmed.toLowerCase() : trimmed.slice(0, spaceIdx).toLowerCase();
    const args = spaceIdx === -1 ? "" : trimmed.slice(spaceIdx + 1);
    logger.info(`[weixin] Slash command: ${command}, args: ${args.slice(0, 50)}`);
    try {
        switch (command) {
            case "/echo":
                await handleEcho(ctx, args, receivedAt, eventTimestamp);
                return { handled: true };
            case "/toggle-debug": {
                const enabled = toggleDebugMode(ctx.accountId);
                await sendReply(ctx, enabled
                    ? "Debug 模式已开启"
                    : "Debug 模式已关闭");
                return { handled: true };
            }
            case "/go2":
                await handleGo2(ctx, args);
                return { handled: true };
            default:
                return { handled: false };
        }
    }
    catch (err) {
        logger.error(`[weixin] Slash command error: ${String(err)}`);
        try {
            await sendReply(ctx, `❌ 指令执行失败: ${String(err).slice(0, 200)}`);
        }
        catch {
            // 发送错误消息也失败了，只能记日志
        }
        return { handled: true };
    }
}
//# sourceMappingURL=slash-commands.js.map