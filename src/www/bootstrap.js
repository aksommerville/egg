window.addEventListener("load", () => {
  const canvas = document.getElementById("egg-canvas");
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "#08f";
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.beginPath();
  ctx.moveTo(0, 0);
  ctx.lineTo(canvas.width, canvas.height);
  ctx.strokeStyle = "#fff";
  ctx.stroke();
}, { once: true });
