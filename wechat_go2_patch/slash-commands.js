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

function normalizeCommandText(content) {
    return content
        .trim()
        .toLowerCase()
        .replace(/[，。！？；：、,.!?;:]/g, " ")
        .replace(/\s+/g, " ");
}
function compactCommandText(content) {
    return normalizeCommandText(content).replace(/\s+/g, "");
}
function mentionsGo2(content, compact) {
    return /\bgo\s*2\b/i.test(content) ||
        compact.includes("go2") ||
        compact.includes("unitree") ||
        compact.includes("宇树") ||
        compact.includes("机器狗") ||
        compact.includes("机器犬");
}
function matchGo2NaturalCommand(content) {
    const normalized = normalizeCommandText(content);
    const compact = compactCommandText(content);
    if (!mentionsGo2(content, compact)) {
        return null;
    }
    if (/(在线|在吗|状态|连接|连上|status|online|ready)/.test(compact)) {
        return "status";
    }
    if (/(恢复站立|恢复起立|自恢复|recoverstand|recoverystand|recover)/.test(compact)) {
        return "recover-stand";
    }
    if (/(停止|停下|别动|不要动|刹车|急停|stop|halt)/.test(compact)) {
        return "stop";
    }
    if (/(平衡站立|balance|balancestand)/.test(compact)) {
        return "balance-stand";
    }
    if (/(坐下|坐姿|sitdown|sit)/.test(compact)) {
        return "sit";
    }
    if (/(坐姿起身|从坐下起来|从坐姿起来|risesit|rise-sit)/.test(compact)) {
        return "rise-sit";
    }
    if (/(趴下|卧倒|站下|standdown|stand-down|liedown)/.test(compact)) {
        return "stand-down";
    }
    if (/(站起来|起立|站立|站好|standup|stand-up)/.test(compact) || normalized.includes("stand up")) {
        return "stand-up";
    }
    if (/(后退|向后|往后|back|backward)/.test(compact)) {
        return "move --vx -0.2 --vy 0.0 --vyaw 0.0 --duration 1.0";
    }
    if (/(前进|向前|往前|走一步|forward)/.test(compact)) {
        return "move --vx 0.2 --vy 0.0 --vyaw 0.0 --duration 1.0";
    }
    if (/(左转|向左转|turnleft|leftturn)/.test(compact) || normalized.includes("turn left")) {
        return "move --vx 0.0 --vy 0.0 --vyaw 0.3 --duration 1.0";
    }
    if (/(右转|向右转|turnright|rightturn)/.test(compact) || normalized.includes("turn right")) {
        return "move --vx 0.0 --vy 0.0 --vyaw -0.3 --duration 1.0";
    }
    return null;
}
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
    const go2NaturalCommand = matchGo2NaturalCommand(trimmed);
    if (go2NaturalCommand) {
        logger.info(`[weixin] Go2 natural command: ${go2NaturalCommand}`);
        await handleGo2(ctx, go2NaturalCommand);
        return { handled: true };
    }
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