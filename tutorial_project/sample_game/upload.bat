..\bin\picotool reboot -f -u
timeout 3
..\bin\picotool load -fx .\build\rp2040.rp2040.rpipico2\sample_game.ino.uf2
