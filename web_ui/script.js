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
        setTimeout(() => addLog('Web control server connected', 'success'), 800);
        setTimeout(() => addLog('Awaiting payload upload...', 'info'), 1300);

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
                    addLog('Rejected file: ' + file.name + ' (not a DLL)', 'error');
                    return;
                }
                currentFile = file;
                fileNameDisplay.textContent = file.name;
                dropZoneContent.classList.add('hidden');
                fileInfo.classList.remove('hidden');
                dropZone.style.cursor = 'default';
                hideStatus();
                addLog('Payload loaded: ' + file.name + ' (' + (file.size / 1024).toFixed(1) + ' KB)', 'success');
            }

            clearBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                const autoRelaunch = document.getElementById('auto-relaunch-toggle').checked;
                injectBtn.disabled = true;
                
                if (autoRelaunch) {
                    showStatus('Closing sandbox & auto-restarting...', 'processing');
                    addLog('Initiating safe unhook sequence...', 'warn');
                    addLog('Auto-relaunch ENABLED: Force closing sandbox & restarting...', 'info');
                } else {
                    showStatus('Force closing sandbox process...', 'processing');
                    addLog('Initiating safe unhook sequence...', 'warn');
                    addLog('Auto-relaunch DISABLED: Force closing sandbox process...', 'info');
                }

                fetch('/unhook?relaunch=' + (autoRelaunch ? 'true' : 'false'), { method: 'POST' })
                .then(r => r.text())
                .then(result => {
                    addLog('Hook detached & sandbox process terminated', 'success');
                    if (autoRelaunch) {
                        addLog('Sandbox process automatically restarted', 'success');
                    } else {
                        addLog('Sandbox process closed cleanly (no restart)', 'success');
                    }
                    resetUI();
                }).catch(() => {
                    addLog('Unhook completed', 'info');
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

                showStatus('Waiting for target window (Sandbox/Controlled Environment)...', 'processing');
                addLog('Uploading DLL binary to injection server...', 'info');

                const reader = new FileReader();
                reader.onload = function(evt) {
                    const arrayBuffer = evt.target.result;
                    addLog('Upload complete (' + arrayBuffer.byteLength + ' bytes)', 'success');
                    addLog('Server is analyzing DLL exports...', 'info');
                    addLog('Standby mode active: Waiting for sandbox/controlled environment target window to open...', 'warn');

                    fetch('/inject', { method: 'POST', body: arrayBuffer })
                    .then(response => response.text())
                    .then(result => {
                        clearBtn.style.pointerEvents = 'auto';
                        clearBtn.style.opacity = '1';

                        if (result === "SUCCESS") {
                            showStatus('Hook active!', 'success');
                            addLog('Export function resolved', 'success');
                            addLog('Target window located', 'success');
                            addLog('SetWindowsHookEx installed on target thread', 'success');
                            addLog('DLL injection completed successfully', 'success');
                            injectBtn.textContent = 'Injected';
                            injectBtn.style.background = 'linear-gradient(135deg, #22c55e, #16a34a)';
                        } else if (result === "NO_EXPORT") {
                            showStatus('No valid export function', 'error');
                            addLog('DLL rejected: no exported functions found (requires at least one non-DllMain export)', 'error');
                            injectBtn.disabled = false;
                        } else if (result === "TARGET_NOT_FOUND") {
                            showStatus('Target window timed out', 'error');
                            addLog('FindWindowW timed out: target window not detected', 'error');
                            addLog('Ensure the target application is running', 'warn');
                            injectBtn.disabled = false;
                        } else if (result === "THREAD_NOT_FOUND") {
                            showStatus('Thread ID lookup failed', 'error');
                            addLog('GetWindowThreadProcessId returned 0', 'error');
                            injectBtn.disabled = false;
                        } else {
                            showStatus('Injection failed: ' + result, 'error');
                            addLog('Hook installation failed: ' + result, 'error');
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
