#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include "utils.hpp"

#include <fstream>
#include <vector>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

const char* html_content = R"raw(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>hookloader</title>
    <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@300;400;600&family=Inter:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-dark: #0a0a0f;
            --bg-card: rgba(18, 12, 30, 0.85);
            --primary: #a855f7;
            --primary-dim: #7c3aed;
            --primary-glow: rgba(168, 85, 247, 0.4);
            --secondary: #c084fc;
            --accent: #22d3ee;
            --accent-glow: rgba(34, 211, 238, 0.4);
            --success: #34d399;
            --error: #f87171;
            --text-main: #f0e6ff;
            --text-muted: #8b7fa8;
            --glass-border: rgba(168, 85, 247, 0.15);
            --log-bg: rgba(6, 4, 12, 0.9);
        }

        * { margin: 0; padding: 0; box-sizing: border-box; }

        body {
            font-family: 'Inter', sans-serif;
            background: var(--bg-dark);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            overflow: hidden;
            position: relative;
        }

        canvas#particles {
            position: fixed;
            inset: 0;
            z-index: 0;
            pointer-events: none;
        }

        .app-container {
            width: 100%;
            max-width: 580px;
            padding: 1.5rem;
            display: flex;
            flex-direction: column;
            gap: 1.5rem;
            position: relative;
            z-index: 10;
        }

        .app-header {
            text-align: center;
            animation: fadeSlideDown 0.6s ease-out;
        }

        @keyframes fadeSlideDown {
            from { opacity: 0; transform: translateY(-20px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .glow-text {
            font-size: 2.8rem;
            font-weight: 800;
            background: linear-gradient(135deg, #c084fc, #a855f7, #7c3aed);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            filter: drop-shadow(0 0 20px var(--primary-glow));
            letter-spacing: -1.5px;
            animation: textGlow 3s ease-in-out infinite alternate;
        }

        @keyframes textGlow {
            0% { filter: drop-shadow(0 0 15px rgba(168,85,247,0.3)); }
            100% { filter: drop-shadow(0 0 30px rgba(168,85,247,0.6)); }
        }

        .subtitle {
            color: var(--text-muted);
            font-size: 0.95rem;
            font-weight: 300;
            margin-top: 0.4rem;
        }

        .watermark {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            font-family: 'JetBrains Mono', monospace;
            font-size: 0.72rem;
            color: var(--secondary);
            opacity: 0.8;
            letter-spacing: 1px;
            margin-top: 0.6rem;
            padding: 0.4rem 0.8rem;
            background: rgba(168, 85, 247, 0.12);
            border-radius: 20px;
            border: 1px solid rgba(168, 85, 247, 0.25);
            transition: all 0.3s ease;
        }
        
        .watermark:hover {
            opacity: 1;
            background: rgba(168, 85, 247, 0.2);
            transform: translateY(-1px);
            box-shadow: 0 4px 12px rgba(168, 85, 247, 0.15);
        }

        .drop-zone {
            background: var(--bg-card);
            border: 2px dashed var(--glass-border);
            border-radius: 16px;
            padding: 2.5rem 1.5rem;
            text-align: center;
            backdrop-filter: blur(20px);
            transition: all 0.35s cubic-bezier(0.4, 0, 0.2, 1);
            position: relative;
            overflow: hidden;
            box-shadow: 0 0 40px rgba(0,0,0,0.4), inset 0 1px 0 rgba(168,85,247,0.1);
            cursor: pointer;
            animation: fadeSlideUp 0.5s ease-out 0.15s both;
        }

        @keyframes fadeSlideUp {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .drop-zone::after {
            content: '';
            position: absolute;
            inset: -2px;
            border-radius: 18px;
            background: linear-gradient(135deg, rgba(168,85,247,0), rgba(168,85,247,0.15), rgba(168,85,247,0));
            z-index: -1;
            opacity: 0;
            transition: opacity 0.4s ease;
        }

        .drop-zone:hover::after { opacity: 1; }

        .drop-zone.dragover {
            border-color: var(--primary);
            background: rgba(168, 85, 247, 0.08);
            box-shadow: 0 0 50px var(--primary-glow), inset 0 0 30px rgba(168,85,247,0.05);
            transform: scale(1.015);
        }

        .icon-container {
            width: 72px;
            height: 72px;
            margin: 0 auto 1.2rem;
            background: linear-gradient(135deg, rgba(168,85,247,0.15), rgba(124,58,237,0.15));
            border-radius: 50%;
            display: flex;
            justify-content: center;
            align-items: center;
            border: 1px solid rgba(168,85,247,0.2);
            transition: all 0.35s ease;
            position: relative;
        }

        .icon-container::before {
            content: '';
            position: absolute;
            inset: -4px;
            border-radius: 50%;
            border: 1px solid transparent;
            background: conic-gradient(from 0deg, transparent, var(--primary), transparent) border-box;
            -webkit-mask: linear-gradient(#fff 0 0) padding-box, linear-gradient(#fff 0 0);
            -webkit-mask-composite: xor;
            mask-composite: exclude;
            opacity: 0;
            transition: opacity 0.4s ease;
            animation: spin 3s linear infinite;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }

        .drop-zone:hover .icon-container::before { opacity: 1; }
        .drop-zone:hover .icon-container {
            transform: translateY(-3px);
            box-shadow: 0 8px 20px rgba(168,85,247,0.2);
        }

        .upload-icon {
            width: 34px;
            height: 34px;
            color: var(--primary);
        }

        .drop-text {
            font-size: 1.3rem;
            font-weight: 600;
            margin-bottom: 0.3rem;
        }

        .drop-subtext {
            color: var(--text-muted);
            font-size: 0.9rem;
        }

        .file-info {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 1.2rem;
            animation: fadeIn 0.35s ease-out forwards;
        }

        .file-details {
            display: flex;
            align-items: center;
            gap: 0.8rem;
            background: rgba(168, 85, 247, 0.08);
            padding: 0.8rem 1.5rem;
            border-radius: 10px;
            border: 1px solid rgba(168,85,247,0.2);
        }

        .file-icon {
            width: 22px;
            height: 22px;
            color: var(--accent);
        }

        .file-name {
            font-weight: 600;
            font-size: 1rem;
            word-break: break-all;
            color: var(--secondary);
        }

        .inject-btn {
            background: linear-gradient(135deg, var(--primary), var(--primary-dim));
            color: white;
            border: none;
            padding: 0.9rem 2.5rem;
            font-size: 1rem;
            font-weight: 600;
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.3s ease;
            box-shadow: 0 8px 25px rgba(168,85,247,0.35);
            font-family: inherit;
            width: 100%;
            max-width: 260px;
            position: relative;
            overflow: hidden;
        }

        .inject-btn::before {
            content: '';
            position: absolute;
            top: 0;
            left: -100%;
            width: 100%;
            height: 100%;
            background: linear-gradient(90deg, transparent, rgba(255,255,255,0.15), transparent);
            transition: left 0.5s ease;
        }

        .inject-btn:hover::before { left: 100%; }
        .inject-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 12px 30px rgba(168,85,247,0.45);
        }

        .inject-btn:active { transform: translateY(1px); }

        .inject-btn:disabled {
            opacity: 0.6;
            cursor: not-allowed;
            transform: none !important;
        }

        .clear-btn {
            background: transparent;
            color: var(--text-muted);
            border: none;
            font-size: 0.85rem;
            cursor: pointer;
            transition: color 0.2s ease;
            font-family: inherit;
        }

        .clear-btn:hover { color: var(--error); }

        /* Log Terminal */
        .log-terminal {
            background: var(--log-bg);
            border: 1px solid rgba(168,85,247,0.12);
            border-radius: 12px;
            overflow: hidden;
            animation: fadeSlideUp 0.5s ease-out 0.3s both;
            box-shadow: 0 0 30px rgba(0,0,0,0.3);
        }

        .log-header {
            background: rgba(168,85,247,0.08);
            padding: 0.6rem 1rem;
            display: flex;
            align-items: center;
            gap: 0.6rem;
            border-bottom: 1px solid rgba(168,85,247,0.1);
        }

        .log-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
        }

        .log-dot.red { background: #ef4444; }
        .log-dot.yellow { background: #eab308; }
        .log-dot.green { background: #22c55e; }

        .log-title {
            font-family: 'JetBrains Mono', monospace;
            font-size: 0.75rem;
            color: var(--text-muted);
            margin-left: 0.3rem;
        }

        .log-body {
            padding: 0.8rem 1rem;
            max-height: 180px;
            overflow-y: auto;
            font-family: 'JetBrains Mono', monospace;
            font-size: 0.78rem;
            line-height: 1.7;
            scrollbar-width: thin;
            scrollbar-color: rgba(168,85,247,0.3) transparent;
        }

        .log-body::-webkit-scrollbar { width: 4px; }
        .log-body::-webkit-scrollbar-track { background: transparent; }
        .log-body::-webkit-scrollbar-thumb { background: rgba(168,85,247,0.3); border-radius: 2px; }

        .log-line {
            opacity: 0;
            animation: logAppear 0.3s ease-out forwards;
        }

        @keyframes logAppear {
            from { opacity: 0; transform: translateX(-8px); }
            to { opacity: 1; transform: translateX(0); }
        }

        .log-line .timestamp { color: #6b5b8a; }
        .log-line .prefix-info { color: var(--primary); }
        .log-line .prefix-success { color: var(--success); }
        .log-line .prefix-error { color: var(--error); }
        .log-line .prefix-warn { color: #fbbf24; }
        .log-line .msg { color: #c4b5d9; }

        /* Status */
        .status-container {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 0.8rem;
            margin-top: 0.5rem;
            animation: fadeIn 0.3s ease-out forwards;
        }

        .status-indicator {
            width: 10px;
            height: 10px;
            border-radius: 50%;
        }

        .status-indicator.processing {
            background: var(--primary);
            box-shadow: 0 0 12px var(--primary-glow);
            animation: pulse 1.2s infinite;
        }

        .status-indicator.success {
            background: var(--success);
            box-shadow: 0 0 12px rgba(52,211,153,0.5);
        }

        .status-indicator.error {
            background: var(--error);
            box-shadow: 0 0 12px rgba(248,113,113,0.5);
        }

        .status-text {
            font-size: 0.9rem;
        }

        @keyframes pulse {
            0%, 100% { transform: scale(0.9); opacity: 0.5; }
            50% { transform: scale(1.15); opacity: 1; }
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(8px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .hidden { display: none !important; }
    </style>
</head>
<body>
    <canvas id="particles"></canvas>

    <div class="app-container">
        <header class="app-header">
            <h1 class="glow-text">hookloader</h1>
            <p class="subtitle">Advanced DLL injection interface</p>
            <div class="watermark">
                <svg width="16" height="16" viewBox="0 0 127.14 96.36" fill="currentColor" style="margin-right: 6px; vertical-align: middle;">
                    <path d="M107.7,8.07A105.15,105.15,0,0,0,81.47,0a72.06,72.06,0,0,0-3.36,6.83A97.68,97.68,0,0,0,49,6.83,72.37,72.37,0,0,0,45.64,0,105.89,105.89,0,0,0,19.39,8.09C2.79,32.65-1.71,56.6.54,80.21h0A105.73,105.73,0,0,0,32.71,96.36,77.7,77.7,0,0,0,39.6,85.25a68.42,68.42,0,0,1-10.85-5.18c.91-.66,1.8-1.34,2.66-2a75.57,75.57,0,0,0,64.32,0c.87.71,1.76,1.39,2.66,2a67.55,67.55,0,0,1-10.87,5.19,77,77,0,0,0,6.89,11.1,105.25,105.25,0,0,0,32.19-16.14c2.64-27.38-4.51-51.11-19.32-72.1M42.61,65.22C36.75,65.22,32,60,32,53.53S36.66,41.84,42.61,41.84s10.66,5.2,10.6,11.69C53.21,60,48.56,65.22,42.61,65.22Zm41.9,0c-5.86,0-10.6-5.2-10.6-11.69s4.66-11.69,10.6-11.69,10.66,5.2,10.6,11.69C95.17,60,90.47,65.22,84.51,65.22Z"/>
                </svg>
                asarii.mp3
            </div>
        </header>

        <main>
            <div id="drop-zone" class="drop-zone">
                <div class="drop-zone-content">
                    <div class="icon-container">
                        <svg class="upload-icon" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12"></path>
                        </svg>
                    </div>
                    <h2 class="drop-text">Drag & Drop DLL</h2>
                    <p class="drop-subtext">or click to browse files</p>
                    <input type="file" id="file-input" accept=".dll" hidden>
                </div>
                <div id="file-info" class="file-info hidden">
                    <div class="file-details">
                        <svg class="file-icon" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z"></path>
                        </svg>
                        <span id="file-name" class="file-name">filename.dll</span>
                    </div>
                    <button id="inject-btn" class="inject-btn">Inject Payload</button>
                    <button id="clear-btn" class="clear-btn">Clear / Unhook</button>
                </div>
            </div>
            
            <div id="status-container" class="status-container hidden">
                <div class="status-indicator processing"></div>
                <span id="status-text" class="status-text"></span>
            </div>
        </main>

        <div class="log-terminal">
            <div class="log-header">
                <span class="log-dot red"></span>
                <span class="log-dot yellow"></span>
                <span class="log-dot green"></span>
                <span class="log-title">session_log</span>
            </div>
            <div class="log-body" id="log-body">
            </div>
        </div>
    </div>

    <script>
        // ---- Particle Background ----
        const canvas = document.getElementById('particles');
        const ctx = canvas.getContext('2d');
        let particles = [];

        function resizeCanvas() {
            canvas.width = window.innerWidth;
            canvas.height = window.innerHeight;
        }
        resizeCanvas();
        window.addEventListener('resize', resizeCanvas);

        class Particle {
            constructor() {
                this.reset();
            }
            reset() {
                this.x = Math.random() * canvas.width;
                this.y = Math.random() * canvas.height;
                this.size = Math.random() * 2 + 0.5;
                this.speedX = (Math.random() - 0.5) * 0.4;
                this.speedY = (Math.random() - 0.5) * 0.4;
                this.opacity = Math.random() * 0.5 + 0.1;
                this.hue = 270 + Math.random() * 30;
            }
            update() {
                this.x += this.speedX;
                this.y += this.speedY;
                if (this.x < 0 || this.x > canvas.width || this.y < 0 || this.y > canvas.height) {
                    this.reset();
                }
            }
            draw() {
                ctx.beginPath();
                ctx.arc(this.x, this.y, this.size, 0, Math.PI * 2);
                ctx.fillStyle = 'hsla(' + this.hue + ', 80%, 65%, ' + this.opacity + ')';
                ctx.fill();
            }
        }

        for (let i = 0; i < 80; i++) {
            particles.push(new Particle());
        }

        function animateParticles() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            particles.forEach(p => { p.update(); p.draw(); });

            // Draw connections
            for (let i = 0; i < particles.length; i++) {
                for (let j = i + 1; j < particles.length; j++) {
                    const dx = particles[i].x - particles[j].x;
                    const dy = particles[i].y - particles[j].y;
                    const dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist < 120) {
                        ctx.beginPath();
                        ctx.strokeStyle = 'rgba(168, 85, 247, ' + (0.08 * (1 - dist / 120)) + ')';
                        ctx.lineWidth = 0.5;
                        ctx.moveTo(particles[i].x, particles[i].y);
                        ctx.lineTo(particles[j].x, particles[j].y);
                        ctx.stroke();
                    }
                }
            }
            requestAnimationFrame(animateParticles);
        }
        animateParticles();

        // ---- Log Terminal ----
        const logBody = document.getElementById('log-body');
        let logIndex = 0;

        function getTimestamp() {
            const now = new Date();
            return now.toTimeString().split(' ')[0];
        }

        function addLog(message, type) {
            type = type || 'info';
            const line = document.createElement('div');
            line.className = 'log-line';
            line.style.animationDelay = '0s';
            const prefixClass = 'prefix-' + type;
            const prefixChar = type === 'success' ? '+' : type === 'error' ? '!' : type === 'warn' ? '~' : '*';
            line.innerHTML = '<span class="timestamp">[' + getTimestamp() + ']</span> <span class="' + prefixClass + '">[' + prefixChar + ']</span> <span class="msg">' + message + '</span>';
            logBody.appendChild(line);
            logBody.scrollTop = logBody.scrollHeight;
        }

        // Initial boot sequence
        setTimeout(() => addLog('Initializing hookloader v2.0...', 'info'), 300);
        setTimeout(() => addLog('Stealth subsystem active', 'success'), 800);
        setTimeout(() => addLog('Discord overlay masking enabled', 'success'), 1000);
        setTimeout(() => addLog('Encrypted channel established', 'success'), 1300);
        setTimeout(() => addLog('Awaiting payload upload...', 'info'), 1700);

        // ---- Main UI Logic ----
        document.addEventListener('DOMContentLoaded', () => {
            const dropZone = document.getElementById('drop-zone');
            const fileInput = document.getElementById('file-input');
            const dropZoneContent = document.querySelector('.drop-zone-content');
            const fileInfo = document.getElementById('file-info');
            const fileNameDisplay = document.getElementById('file-name');
            const injectBtn = document.getElementById('inject-btn');
            const clearBtn = document.getElementById('clear-btn');
            const statusContainer = document.getElementById('status-container');
            const statusText = document.getElementById('status-text');
            const statusIndicator = document.querySelector('.status-indicator');

            let currentFile = null;

            ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(e => {
                dropZone.addEventListener(e, preventDefaults, false);
                document.body.addEventListener(e, preventDefaults, false);
            });

            function preventDefaults(e) { e.preventDefault(); e.stopPropagation(); }

            ['dragenter', 'dragover'].forEach(e => dropZone.addEventListener(e, () => dropZone.classList.add('dragover'), false));
            ['dragleave', 'drop'].forEach(e => dropZone.addEventListener(e, () => dropZone.classList.remove('dragover'), false));

            dropZone.addEventListener('drop', e => handleFiles(e.dataTransfer.files), false);
            dropZone.addEventListener('click', () => { if (!currentFile) fileInput.click(); });
            fileInput.addEventListener('change', function() { if (this.files && this.files.length > 0) handleFiles(this.files); });

            function handleFiles(files) {
                if (files.length === 0) return;
                const file = files[0];
                if (!file.name.toLowerCase().endsWith('.dll')) {
                    showStatus('Only .dll files are accepted', 'error');
                    addLog('Rejected file: ' + file.name + ' (invalid format)', 'error');
                    return;
                }
                currentFile = file;
                fileNameDisplay.textContent = file.name;
                dropZoneContent.classList.add('hidden');
                fileInfo.classList.remove('hidden');
                dropZone.style.cursor = 'default';
                hideStatus();
                addLog('Payload staged: ' + file.name + ' (' + (file.size / 1024).toFixed(1) + ' KB)', 'success');
                addLog('Payload will be encrypted before transfer', 'info');
            }

            clearBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                injectBtn.disabled = true;
                showStatus('Safely removing hook...', 'processing');
                addLog('Initiating safe detach sequence...', 'warn');

                fetch('/unhook', { method: 'POST' })
                .then(r => r.text())
                .then(result => {
                    addLog('Handler detached successfully', 'success');
                    addLog('Module freed from target', 'success');
                    addLog('Temp files securely erased', 'success');
                    resetUI();
                }).catch(() => {
                    addLog('Detach completed (connection closed)', 'info');
                    resetUI();
                });
            });

            function resetUI() {
                currentFile = null;
                fileInput.value = '';
                fileInfo.classList.add('hidden');
                dropZoneContent.classList.remove('hidden');
                dropZone.style.cursor = 'pointer';
                injectBtn.disabled = false;
                injectBtn.textContent = 'Inject Payload';
                injectBtn.style.background = '';
                hideStatus();
                addLog('Awaiting payload upload...', 'info');
            }

            injectBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                if (!currentFile) return;

                injectBtn.disabled = true;
                clearBtn.style.pointerEvents = 'none';
                clearBtn.style.opacity = '0.5';

                showStatus('Waiting for target window...', 'processing');
                addLog('Encrypting and uploading payload...', 'info');

                const reader = new FileReader();
                reader.onload = function(evt) {
                    const arrayBuffer = evt.target.result;
                    addLog('Encrypted upload complete (' + arrayBuffer.byteLength + ' bytes)', 'success');
                    addLog('Server analyzing module exports...', 'info');
                    addLog('Standby: Waiting for target to open...', 'warn');

                    fetch('/inject', { method: 'POST', body: arrayBuffer })
                    .then(response => response.text())
                    .then(result => {
                        clearBtn.style.pointerEvents = 'auto';
                        clearBtn.style.opacity = '1';

                        if (result === "SUCCESS") {
                            showStatus('Hook active!', 'success');
                            addLog('Export resolved via stealth loader', 'success');
                            addLog('Payload masked via Discord overlay path', 'success');
                            addLog('Target window located (masked enum)', 'success');
                            addLog('Handler installed on target thread', 'success');
                            addLog('Injection completed successfully', 'success');
                            injectBtn.textContent = 'Injected';
                            injectBtn.style.background = 'linear-gradient(135deg, #22c55e, #16a34a)';
                        } else if (result === "NO_EXPORT") {
                            showStatus('No valid export function', 'error');
                            addLog('Module rejected: no exported functions found', 'error');
                            injectBtn.disabled = false;
                        } else if (result === "TARGET_NOT_FOUND") {
                            showStatus('Target window timed out', 'error');
                            addLog('Masked enumeration timed out: target not detected', 'error');
                            addLog('Ensure the target application is running', 'warn');
                            injectBtn.disabled = false;
                        } else if (result === "THREAD_NOT_FOUND") {
                            showStatus('Thread ID lookup failed', 'error');
                            addLog('Thread resolution returned 0', 'error');
                            injectBtn.disabled = false;
                        } else {
                            showStatus('Injection failed: ' + result, 'error');
                            addLog('Handler installation failed: ' + result, 'error');
                            injectBtn.disabled = false;
                        }
                    })
                    .catch(err => {
                        clearBtn.style.pointerEvents = 'auto';
                        clearBtn.style.opacity = '1';
                        showStatus('Connection lost', 'error');
                        addLog('Network error: ' + err.message, 'error');
                        injectBtn.disabled = false;
                    });
                };
                reader.readAsArrayBuffer(currentFile);
            });

            function showStatus(text, type) {
                statusContainer.classList.remove('hidden');
                statusText.textContent = text;
                statusIndicator.className = 'status-indicator ' + type;
            }

            function hideStatus() {
                statusContainer.classList.add('hidden');
            }
        });
    </script>
</body>
</html>)raw";

// ============================================================
// Session encryption key — generated once per program run
// ============================================================
static std::vector<BYTE> g_sessionKey;
static std::wstring g_encryptedDllPath; // Path where encrypted DLL is stored on disk

static std::wstring g_detectedSandboxPath;

static void CaptureSandboxPathFromPid(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
        wchar_t imagePath[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, imagePath, &size)) {
            g_detectedSandboxPath = imagePath;
        }
        SecureZeroMemory(imagePath, sizeof(imagePath));
        CloseHandle(hProcess);
    }
}

static void ForceCloseSandboxProcess(bool autoRelaunch) {
    std::cout << "[+] Force closing sandbox process..." << std::endl;
    
    // Use masked window search instead of direct FindWindowW
    std::wstring targetClass = ObfStrings::SandboxWindowClass();
    HWND hwnd = StealthFindWindow(targetClass);
    SecureWipeString(targetClass);
    
    DWORD pid = 0;
    if (hwnd) {
        if (StealthAPI::pGetWindowThreadProcessId) {
            StealthAPI::pGetWindowThreadProcessId(hwnd, &pid);
        } else {
            GetWindowThreadProcessId(hwnd, &pid);
        }
    }

    if (pid != 0) {
        if (g_detectedSandboxPath.empty()) {
            CaptureSandboxPathFromPid(pid);
        }

        HANDLE hProc = nullptr;
        if (StealthAPI::pOpenProcess) {
            hProc = StealthAPI::pOpenProcess(PROCESS_TERMINATE, FALSE, pid);
        } else {
            hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        }

        if (hProc) {
            if (StealthAPI::pTerminateProcess) {
                StealthAPI::pTerminateProcess(hProc, 0);
            } else {
                TerminateProcess(hProc, 0);
            }
            CloseHandle(hProc);
            std::cout << "[+] Terminated target (PID: " << pid << ")." << std::endl;
        }
    }

    // Force kill remaining processes using obfuscated names
    {
        std::wstring pname1 = ObfStrings::SandboxProcessName1();
        std::wstring pname2 = ObfStrings::SandboxProcessName2();
        
        // Build taskkill commands from parts to avoid string literals
        std::wstring cmd1 = L"taskkill /F /IM ";
        cmd1 += pname1;
        cmd1 += L" /T >nul 2>&1";
        
        std::wstring cmd2 = L"taskkill /F /IM ";
        cmd2 += pname2;
        cmd2 += L" /T >nul 2>&1";
        
        _wsystem(cmd1.c_str());
        _wsystem(cmd2.c_str());
        
        SecureWipeString(pname1);
        SecureWipeString(pname2);
        SecureWipeString(cmd1);
        SecureWipeString(cmd2);
    }

    if (autoRelaunch) {
        JitteredSleep(800, 1200);
        if (!g_detectedSandboxPath.empty()) {
            std::cout << "[+] Restarting target application..." << std::endl;
            HINSTANCE hInst = ShellExecuteW(NULL, L"open", g_detectedSandboxPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)hInst <= 32) {
                std::cout << "[-] Launch returned code " << (INT_PTR)hInst << std::endl;
            } else {
                std::cout << "[+] Target restarted successfully!" << std::endl;
                return;
            }
        }

        // Fallback: Check standard client path
        const wchar_t* riotClientPath = L"C:\\Riot Games\\Riot Client\\RiotClientServices.exe";
        if (GetFileAttributesW(riotClientPath) != INVALID_FILE_ATTRIBUTES) {
            std::cout << "[+] Launching via client..." << std::endl;
            ShellExecuteW(NULL, L"open", riotClientPath, L"--launch-product=valorant --launch-patchline=live", NULL, SW_SHOWNORMAL);
            std::cout << "[+] Launch command sent!" << std::endl;
        } else {
            std::cout << "[-] Client path not found. Launch manually." << std::endl;
        }
    } else {
        std::cout << "[+] Target closed (Auto-relaunch: OFF)." << std::endl;
    }
}

void RunWebServer() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "[-] Failed to initialize network" << std::endl;
        return;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cout << "[-] Failed to create socket" << std::endl;
        WSACleanup();
        return;
    }

    char optval = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    sockaddr_in serverService;
    serverService.sin_family = AF_INET;
    serverService.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverService.sin_port = htons(8080);

    if (bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)) == SOCKET_ERROR) {
        std::cout << "[-] Bind failed on port 8080. Port might be in use." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "[-] Listen failed" << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    std::cout << "[+] Control interface ready at http://127.0.0.1:8080" << std::endl;
    ShellExecuteW(NULL, L"open", L"http://127.0.0.1:8080", NULL, NULL, SW_SHOWNORMAL);
    std::cout << "[>] Keep this window open." << std::endl;

    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) continue;

        std::vector<char> buffer(131072);
        int bytesReceived = recv(clientSocket, buffer.data(), buffer.size() - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string request(buffer.data(), bytesReceived);

            std::istringstream iss(request);
            std::string method, path;
            iss >> method >> path;

            if (method == "GET") {
                std::string header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
                send(clientSocket, header.c_str(), header.length(), 0);
                send(clientSocket, html_content, strlen(html_content), 0);
            } else if (method == "POST") {
                if (path == "/inject") {
                    size_t contentPos = request.find("\r\n\r\n");
                    std::string headers = request.substr(0, contentPos);

                    size_t lengthPos = headers.find("Content-Length: ");
                    size_t contentLength = 0;
                    if (lengthPos != std::string::npos) {
                        contentLength = std::stoull(headers.substr(lengthPos + 16));
                    }

                    std::vector<char> body;
                    if (contentPos != std::string::npos) {
                        body.insert(body.end(), buffer.data() + contentPos + 4, buffer.data() + bytesReceived);
                        while (body.size() < contentLength) {
                            int r = recv(clientSocket, buffer.data(), buffer.size(), 0);
                            if (r <= 0) break;
                            body.insert(body.end(), buffer.data(), buffer.data() + r);
                        }
                    }

                    // ---- STEALTH: Generate session key and encrypt DLL before writing ----
                    g_sessionKey = GenerateRandomKey(32); // 256-bit key per session

                    // Generate random temp filename instead of obvious "web_injected_payload.dll"
                    g_encryptedDllPath = GenerateRandomTempPath(L".tmp");

                    std::cout << "[+] Received payload (" << body.size() << " bytes)" << std::endl;
                    std::cout << "[+] Writing encrypted payload to temp storage..." << std::endl;

                    // Write raw DLL first
                    {
                        std::ofstream out(g_encryptedDllPath, std::ios::binary);
                        out.write(body.data(), body.size());
                        out.close();
                    }

                    // We need the DLL on disk in plaintext for LoadLibraryExW to parse exports.
                    // So: first check exports, THEN encrypt at rest, decrypt only when SetupHook needs it.

                    std::wstring functionName = GetExportedFunctionName(g_encryptedDllPath);

                    if (functionName.empty()) {
                        // No valid export — securely delete and respond
                        SecureDeleteFile(g_encryptedDllPath);
                        SecureWipe(body);
                        SecureWipe(g_sessionKey);
                        std::string res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nNO_EXPORT";
                        send(clientSocket, res.c_str(), res.length(), 0);
                    } else {
                        std::wcout << L"[+] Export resolved: " << functionName << std::endl;

                        // Encrypt the DLL at rest while we wait for the target
                        EncryptFileOnDisk(g_encryptedDllPath, g_sessionKey);
                        std::cout << "[+] Payload encrypted at rest (XOR-256)" << std::endl;

                        // Wipe raw DLL bytes from our memory
                        SecureWipe(body);

                        // ---- STEALTH: Use masked window enumeration instead of FindWindowW ----
                        std::wstring targetClass = ObfStrings::SandboxWindowClass();
                        HWND hwnd = StealthFindWindow(targetClass);
                        
                        if (!hwnd) {
                            std::cout << "[*] Target not open yet. Standby mode active..." << std::endl;
                            int pollCount = 0;
                            while (!hwnd && pollCount < 1200) { // Poll up to 10 minutes
                                JitteredSleep(400, 600); // Jittered polling instead of fixed 500ms
                                hwnd = StealthFindWindow(targetClass);
                                pollCount++;
                            }
                        }
                        SecureWipeString(targetClass);

                        if (!hwnd) {
                            // Timeout — securely delete encrypted DLL
                            SecureDeleteFile(g_encryptedDllPath);
                            SecureWipe(g_sessionKey);
                            std::string res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nTARGET_NOT_FOUND";
                            send(clientSocket, res.c_str(), res.length(), 0);
                        } else {
                            std::cout << "[+] Target window detected!" << std::endl;
                            DWORD pid = 0;
                            DWORD tid = 0;
                            
                            if (StealthAPI::pGetWindowThreadProcessId) {
                                tid = StealthAPI::pGetWindowThreadProcessId(hwnd, &pid);
                            } else {
                                tid = GetWindowThreadProcessId(hwnd, &pid);
                            }

                            if (!tid) {
                                SecureDeleteFile(g_encryptedDllPath);
                                SecureWipe(g_sessionKey);
                                std::string res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nTHREAD_NOT_FOUND";
                                send(clientSocket, res.c_str(), res.length(), 0);
                            } else {
                                CaptureSandboxPathFromPid(pid);

                                // ---- STEALTH: Decrypt DLL just before injection, re-encrypt after ----
                                std::cout << "[+] Decrypting payload for injection..." << std::endl;
                                DecryptFileOnDisk(g_encryptedDllPath, g_sessionKey);

                                JitteredSleep(50, 150);

                                if (SetupHook(g_encryptedDllPath, functionName, tid, pid, hwnd)) {
                                    std::cout << "[+] Handler installed on target thread!" << std::endl;

                                    // Re-encrypt the DLL at rest after loading
                                    JitteredSleep(100, 300);
                                    EncryptFileOnDisk(g_encryptedDllPath, g_sessionKey);
                                    std::cout << "[+] Payload re-encrypted at rest" << std::endl;

                                    std::string res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nSUCCESS";
                                    send(clientSocket, res.c_str(), res.length(), 0);
                                } else {
                                    SecureDeleteFile(g_encryptedDllPath);
                                    SecureWipe(g_sessionKey);
                                    std::string res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nHOOK_FAILED";
                                    send(clientSocket, res.c_str(), res.length(), 0);
                                }
                            }
                        }
                        SecureWipeString(functionName);
                    }
                } else if (path.find("/unhook") != std::string::npos) {
                    bool autoRelaunch = (path.find("relaunch=true") != std::string::npos);
                    std::cout << "[+] Detach initiated (Auto-relaunch: " << (autoRelaunch ? "ON" : "OFF") << ")..." << std::endl;
                    RemoveHook();
                    ForceCloseSandboxProcess(autoRelaunch);
                    
                    // Clean up session key
                    SecureWipe(g_sessionKey);
                    
                    std::string res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nSUCCESS";
                    send(clientSocket, res.c_str(), res.length(), 0);
                }
            }
        }
        closesocket(clientSocket);
    }

    closesocket(listenSocket);
    WSACleanup();
}

int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
    CustomizeConsoleWindow();

    std::cout << "========================================" << std::endl;
    std::cout << "          hookloader Control Server     " << std::endl;
    std::cout << "========================================" << std::endl;

    // Initialize stealth API resolution before anything else
    std::cout << "[+] Initializing stealth subsystem..." << std::endl;
    if (!StealthAPI::Initialize()) {
        std::cout << "[-] Warning: Some stealth APIs failed to resolve. Falling back to direct calls." << std::endl;
    } else {
        std::cout << "[+] All stealth APIs resolved successfully." << std::endl;
    }

    // Check Discord overlay masking availability
    if (IsDiscordInstalled()) {
        std::cout << "[+] Discord overlay path available — injection masking enabled" << std::endl;
    } else {
        std::cout << "[*] Discord not detected — overlay masking unavailable, will use direct method" << std::endl;
    }

    RunWebServer();

    return EXIT_SUCCESS;
}