import Phaser from "phaser";
import "./style.css";

type Stance = "eaves" | "flower";
type RoundState = "waiting" | "playing" | "victory" | "defeat";
type ActionName = "light" | "heavy" | "dodge" | "stance";

interface DemoSnapshot {
  round: RoundState;
  playerHealth: number;
  enemyHealth: number;
  posture: number;
  stance: Stance;
  playerX: number;
  playerY: number;
  enemyX: number;
  enemyY: number;
  enemyTelegraphing: boolean;
  lastAction: string;
}

declare global {
  interface Window {
    __f1Demo?: {
      state: () => DemoSnapshot;
      start: () => void;
      restart: () => void;
      action: (name: ActionName) => void;
    };
  }
}

const ui = {
  startModal: document.querySelector<HTMLElement>("#start-modal")!,
  startButton: document.querySelector<HTMLButtonElement>("#start-button")!,
  resultModal: document.querySelector<HTMLElement>("#result-modal")!,
  restartButton: document.querySelector<HTMLButtonElement>("#restart-button")!,
  resultTitle: document.querySelector<HTMLElement>("#result-title")!,
  resultCopy: document.querySelector<HTMLElement>("#result-copy")!,
  playerHealth: document.querySelector<HTMLElement>("#player-health-bar")!,
  enemyHealth: document.querySelector<HTMLElement>("#enemy-health-bar")!,
  posture: document.querySelector<HTMLElement>("#posture-bar")!,
  stance: document.querySelector<HTMLElement>("#stance-label")!,
  objective: document.querySelector<HTMLElement>("#objective")!,
  callout: document.querySelector<HTMLElement>("#combat-callout")!,
  actionButtons: [...document.querySelectorAll<HTMLButtonElement>("[data-action]")]
};

let audioContext: AudioContext | null = null;

function tone(frequency: number, duration = 0.07, gainValue = 0.045): void {
  if (!audioContext) return;
  const oscillator = audioContext.createOscillator();
  const gain = audioContext.createGain();
  oscillator.type = "triangle";
  oscillator.frequency.value = frequency;
  gain.gain.setValueAtTime(gainValue, audioContext.currentTime);
  gain.gain.exponentialRampToValueAtTime(0.0001, audioContext.currentTime + duration);
  oscillator.connect(gain).connect(audioContext.destination);
  oscillator.start();
  oscillator.stop(audioContext.currentTime + duration);
}

class DemoScene extends Phaser.Scene {
  private player!: Phaser.GameObjects.Image;
  private enemy!: Phaser.GameObjects.Image;
  private playerShadow!: Phaser.GameObjects.Ellipse;
  private enemyShadow!: Phaser.GameObjects.Ellipse;
  private telegraph!: Phaser.GameObjects.Ellipse;
  private keys!: Record<"W" | "A" | "S" | "D" | "J" | "K" | "Q" | "R", Phaser.Input.Keyboard.Key>;
  private space!: Phaser.Input.Keyboard.Key;
  private round: RoundState = "waiting";
  private stance: Stance = "eaves";
  private playerHealth = 100;
  private enemyHealth = 100;
  private posture = 58;
  private actionUntil = 0;
  private attackReadyAt = 0;
  private invulnerableUntil = 0;
  private enemyAttackReadyAt = 0;
  private enemyTelegraphing = false;
  private lastMove = new Phaser.Math.Vector2(1, 0);
  private lastAction = "等待开始";
  private calloutTimer?: number;

  constructor() {
    super("demo");
  }

  preload(): void {
    this.load.image("arena", "./assets/environment/umbrella-lane-clean.png");
    this.load.image("player", "./assets/characters/player.png");
    this.load.image("enemy", "./assets/characters/enforcer.png");
  }

  create(): void {
    this.add.image(640, 360, "arena").setDisplaySize(1280, 720).setDepth(-100);
    this.add.rectangle(640, 360, 1280, 720, 0x071018, 0.08).setDepth(-90);

    this.playerShadow = this.add.ellipse(410, 584, 92, 25, 0x000000, 0.54).setDepth(500);
    this.enemyShadow = this.add.ellipse(875, 566, 122, 31, 0x000000, 0.64).setDepth(500);
    this.telegraph = this.add
      .ellipse(875, 566, 164, 58, 0x8f211d, 0.12)
      .setStrokeStyle(3, 0xd55643, 0.9)
      .setDepth(490)
      .setVisible(false);

    this.player = this.add.image(410, 594, "player").setOrigin(0.5, 0.96).setDepth(600);
    this.enemy = this.add.image(875, 578, "enemy").setOrigin(0.5, 0.96).setDepth(600);

    this.keys = this.input.keyboard!.addKeys("W,A,S,D,J,K,Q,R") as typeof this.keys;
    this.space = this.input.keyboard!.addKey(Phaser.Input.Keyboard.KeyCodes.SPACE);

    this.input.keyboard!.on("keydown-J", () => this.performAction("light"));
    this.input.keyboard!.on("keydown-K", () => this.performAction("heavy"));
    this.input.keyboard!.on("keydown-Q", () => this.performAction("stance"));
    this.space.on("down", () => this.performAction("dodge"));
    this.input.keyboard!.on("keydown-R", () => this.restartRound());
    this.input.keyboard!.on("keydown-W", () => this.nudge(0, -1));
    this.input.keyboard!.on("keydown-A", () => this.nudge(-1, 0));
    this.input.keyboard!.on("keydown-S", () => this.nudge(0, 1));
    this.input.keyboard!.on("keydown-D", () => this.nudge(1, 0));

    this.resetActors();
    this.publish();

    window.__f1Demo = {
      state: () => this.snapshot(),
      start: () => this.startRound(),
      restart: () => this.restartRound(),
      action: (name) => this.performAction(name)
    };
  }

  update(time: number, delta: number): void {
    if (this.round !== "playing") {
      this.updatePresentation();
      return;
    }

    if (time >= this.actionUntil) this.updateMovement(delta / 1000);
    this.updateEnemy(time, delta / 1000);
    this.updatePresentation();
  }

  startRound(): void {
    if (this.round === "playing") return;
    if (!audioContext) audioContext = new AudioContext();
    void audioContext.resume();
    this.round = "playing";
    this.lastAction = "进入伞巷";
    this.enemyAttackReadyAt = this.time.now + 1600;
    ui.startModal.hidden = true;
    ui.resultModal.hidden = true;
    this.showCallout("镇守苏醒");
    this.publish();
  }

  restartRound(): void {
    this.tweens.killAll();
    this.time.removeAllEvents();
    this.resetActors();
    this.round = "playing";
    this.lastAction = "重新演示";
    this.enemyAttackReadyAt = this.time.now + 1600;
    ui.startModal.hidden = true;
    ui.resultModal.hidden = true;
    this.showCallout("战斗重置");
    this.publish();
  }

  performAction(name: ActionName): void {
    if (this.round !== "playing") return;
    const now = this.time.now;
    if (name === "stance") {
      if (now < this.actionUntil) return;
      this.stance = this.stance === "eaves" ? "flower" : "eaves";
      this.lastAction = this.stance === "eaves" ? "切换：檐卫势" : "切换：花转势";
      this.posture = Math.min(100, this.posture + 8);
      this.player.setTint(this.stance === "eaves" ? 0xf0d49a : 0xa9e1de);
      this.time.delayedCall(150, () => this.player.clearTint());
      tone(this.stance === "eaves" ? 185 : 245, 0.09, 0.025);
      this.showCallout(this.stance === "eaves" ? "檐卫势" : "花转势");
      this.publish();
      return;
    }
    if (name === "dodge") {
      if (now < this.actionUntil || now < this.attackReadyAt) return;
      this.dodge(now);
      return;
    }
    if (now < this.actionUntil || now < this.attackReadyAt) return;
    if (name === "heavy" && this.posture < 22) {
      this.showCallout("架势不足");
      return;
    }
    this.attack(name, now);
  }

  snapshot(): DemoSnapshot {
    return {
      round: this.round,
      playerHealth: this.playerHealth,
      enemyHealth: this.enemyHealth,
      posture: this.posture,
      stance: this.stance,
      playerX: Math.round(this.player.x),
      playerY: Math.round(this.player.y),
      enemyX: Math.round(this.enemy.x),
      enemyY: Math.round(this.enemy.y),
      enemyTelegraphing: this.enemyTelegraphing,
      lastAction: this.lastAction
    };
  }

  private resetActors(): void {
    this.round = "waiting";
    this.stance = "eaves";
    this.playerHealth = 100;
    this.enemyHealth = 100;
    this.posture = 58;
    this.actionUntil = 0;
    this.attackReadyAt = 0;
    this.invulnerableUntil = 0;
    this.enemyAttackReadyAt = 0;
    this.enemyTelegraphing = false;
    this.lastMove.set(1, 0);
    this.player.setPosition(410, 594).setAlpha(1).setAngle(0).setFlipX(false).clearTint().setVisible(true);
    this.enemy.setPosition(875, 578).setAlpha(1).setAngle(0).setFlipX(false).clearTint().setVisible(true);
    this.telegraph.setVisible(false).setAlpha(1).setScale(1);
    this.updatePresentation();
  }

  private updateMovement(dt: number): void {
    const direction = new Phaser.Math.Vector2(
      Number(this.keys.D.isDown) - Number(this.keys.A.isDown),
      Number(this.keys.S.isDown) - Number(this.keys.W.isDown)
    );
    if (direction.lengthSq() === 0) return;
    direction.normalize();
    this.lastMove.copy(direction);
    const speed = 235;
    this.player.x = Phaser.Math.Clamp(this.player.x + direction.x * speed * dt, 275, 1080);
    this.player.y = Phaser.Math.Clamp(this.player.y + direction.y * speed * 0.62 * dt, 465, 650);
    if (Math.abs(direction.x) > 0.1) this.player.setFlipX(direction.x < 0);
    this.lastAction = "斜向移动";
  }

  private nudge(x: number, y: number): void {
    if (this.round !== "playing" || this.time.now < this.actionUntil) return;
    this.lastMove.set(x, y);
    this.player.x = Phaser.Math.Clamp(this.player.x + x * 14, 275, 1080);
    this.player.y = Phaser.Math.Clamp(this.player.y + y * 9, 465, 650);
    if (x !== 0) this.player.setFlipX(x < 0);
    this.lastAction = "斜向移动";
    this.publish();
  }

  private attack(kind: "light" | "heavy", now: number): void {
    const heavy = kind === "heavy";
    const flowerBonus = this.stance === "flower" && !heavy;
    const eavesBonus = this.stance === "eaves" && heavy;
    const duration = heavy ? 560 : 260;
    this.actionUntil = now + duration;
    this.attackReadyAt = now + (heavy ? 720 : flowerBonus ? 235 : 330);
    this.posture = Phaser.Math.Clamp(this.posture + (heavy ? -22 : 8), 0, 100);
    this.lastAction = heavy ? "重击蓄势" : flowerBonus ? "花转连击" : "轻击";

    const direction = this.enemy.x >= this.player.x ? 1 : -1;
    this.player.setFlipX(direction < 0);
    const originX = this.player.x;
    this.tweens.add({
      targets: this.player,
      x: Phaser.Math.Clamp(originX + direction * (heavy ? 54 : 30), 275, 1080),
      angle: direction * (heavy ? 8 : 4),
      duration: heavy ? 180 : 90,
      yoyo: true,
      ease: heavy ? "Cubic.In" : "Quad.Out",
      onComplete: () => this.player.setAngle(0)
    });
    this.drawAttackArc(heavy, direction);
    tone(heavy ? 72 : 128, heavy ? 0.13 : 0.07, heavy ? 0.06 : 0.035);

    this.time.delayedCall(heavy ? 190 : 80, () => {
      if (this.round !== "playing" || this.enemyHealth <= 0) return;
      const range = heavy ? 300 : 225;
      if (this.combatDistance() > range) {
        this.lastAction = heavy ? "重击落空" : "轻击落空";
        this.publish();
        return;
      }
      const damage = heavy ? (eavesBonus ? 36 : 29) : (flowerBonus ? 18 : 13);
      this.damageEnemy(damage, heavy);
    });
    this.publish();
  }

  private dodge(now: number): void {
    const direction = this.lastMove.lengthSq() > 0 ? this.lastMove.clone() : new Phaser.Math.Vector2(-1, 0);
    const targetX = Phaser.Math.Clamp(this.player.x + direction.x * 155, 275, 1080);
    const targetY = Phaser.Math.Clamp(this.player.y + direction.y * 96, 465, 650);
    this.actionUntil = now + 310;
    this.attackReadyAt = now + 360;
    this.invulnerableUntil = now + 1050;
    this.lastAction = "闪避无敌";
    this.tweens.add({
      targets: this.player,
      x: targetX,
      y: targetY,
      alpha: 0.48,
      duration: 150,
      yoyo: true,
      ease: "Cubic.Out"
    });
    tone(210, 0.06, 0.025);
    this.showCallout("穿位");
    this.publish();
  }

  private updateEnemy(time: number, dt: number): void {
    if (this.enemyHealth <= 0 || this.enemyTelegraphing || time < this.enemyAttackReadyAt) return;
    const distance = this.combatDistance();
    if (distance <= 280) {
      this.beginEnemyTelegraph();
      return;
    }
    const dx = this.player.x - this.enemy.x;
    const dy = this.player.y - this.enemy.y;
    const length = Math.max(1, Math.hypot(dx, dy * 1.6));
    this.enemy.x = Phaser.Math.Clamp(this.enemy.x + (dx / length) * 62 * dt, 320, 1060);
    this.enemy.y = Phaser.Math.Clamp(this.enemy.y + (dy / length) * 40 * dt, 470, 630);
  }

  private beginEnemyTelegraph(): void {
    this.enemyTelegraphing = true;
    this.lastAction = "敌人蓄力";
    this.telegraph.setPosition(this.enemy.x, this.enemy.y - 10).setVisible(true).setScale(0.75).setAlpha(0.8);
    this.tweens.add({ targets: this.telegraph, scaleX: 1.3, scaleY: 1.3, alpha: 0.22, duration: 900 });
    this.enemy.setTint(0xc57a68);
    this.tweens.add({ targets: this.enemy, alpha: 0.76, duration: 225, yoyo: true, repeat: 2 });
    this.showCallout("镇守蓄力 · 闪避！", 940);
    tone(54, 0.4, 0.035);
    this.time.delayedCall(900, () => this.resolveEnemyAttack());
    this.publish();
  }

  private resolveEnemyAttack(): void {
    if (this.round !== "playing" || this.enemyHealth <= 0) return;
    this.enemyTelegraphing = false;
    this.telegraph.setVisible(false);
    this.enemy.setAlpha(1).clearTint();
    this.enemyAttackReadyAt = this.time.now + 2200;
    this.drawEnemyStrike();
    if (this.time.now < this.invulnerableUntil || this.combatDistance() > 285) {
      this.lastAction = "闪避成功";
      this.showCallout("看破");
      tone(260, 0.08, 0.025);
      this.publish();
      return;
    }
    this.playerHealth = Math.max(0, this.playerHealth - 14);
    this.posture = Math.max(0, this.posture - 14);
    this.lastAction = "受到镇守重击";
    this.player.setTintFill(0xe7a394);
    this.cameras.main.shake(150, 0.012);
    this.tweens.add({
      targets: this.player,
      x: Phaser.Math.Clamp(
        this.player.x - (this.enemy.x > this.player.x ? 55 : -55),
        275,
        1080
      ),
      duration: 150,
      ease: "Cubic.Out"
    });
    this.time.delayedCall(120, () => this.player.clearTint());
    tone(63, 0.18, 0.07);
    if (this.playerHealth <= 0) this.finishRound(false);
    this.publish();
  }

  private damageEnemy(damage: number, heavy: boolean): void {
    this.enemyHealth = Math.max(0, this.enemyHealth - damage);
    this.lastAction = heavy ? `重击命中 · ${damage}` : `轻击命中 · ${damage}`;
    this.enemy.setTintFill(heavy ? 0xffd3a3 : 0xc6edf0);
    this.cameras.main.shake(heavy ? 120 : 58, heavy ? 0.01 : 0.004);
    const knockback = this.enemy.x >= this.player.x ? 1 : -1;
    this.tweens.add({
      targets: this.enemy,
      x: Phaser.Math.Clamp(this.enemy.x + knockback * (heavy ? 68 : 24), 320, 1060),
      angle: knockback * (heavy ? 5 : 2),
      duration: heavy ? 180 : 90,
      yoyo: true,
      ease: "Cubic.Out",
      onComplete: () => this.enemy.setAngle(0)
    });
    this.time.delayedCall(100, () => this.enemy.clearTint());
    this.showCallout(heavy ? "破势" : "命中");
    if (this.enemyHealth <= 0) this.finishRound(true);
    this.publish();
  }

  private finishRound(victory: boolean): void {
    this.round = victory ? "victory" : "defeat";
    this.enemyTelegraphing = false;
    this.telegraph.setVisible(false);
    this.lastAction = victory ? "镇守已破" : "演示失败";
    const target = victory ? this.enemy : this.player;
    this.tweens.add({ targets: target, alpha: 0, y: target.y + 28, duration: 520, ease: "Quad.In" });
    this.time.delayedCall(500, () => {
      ui.resultTitle.textContent = victory ? "镇守已破" : "沈砚倒下";
      ui.resultCopy.textContent = victory
        ? "移动、判定、受击、闪避和胜负已经由程序实际运行。"
        : "观察朱砂警示圈，在镇守落击前使用空格闪避。";
      ui.resultModal.hidden = false;
    });
    tone(victory ? 164 : 48, 0.45, 0.045);
    this.publish();
  }

  private combatDistance(): number {
    return Math.hypot(this.enemy.x - this.player.x, (this.enemy.y - this.player.y) * 1.75);
  }

  private updatePresentation(): void {
    const playerScale = Phaser.Math.Linear(0.128, 0.17, (this.player.y - 465) / 185);
    const enemyScale = Phaser.Math.Linear(0.145, 0.205, (this.enemy.y - 470) / 160);
    this.player.setScale(playerScale).setDepth(Math.round(this.player.y));
    this.enemy.setScale(enemyScale).setDepth(Math.round(this.enemy.y));
    this.playerShadow
      .setPosition(this.player.x, this.player.y - 7)
      .setScale(playerScale / 0.17)
      .setDepth(Math.round(this.player.y) - 2);
    this.enemyShadow
      .setPosition(this.enemy.x, this.enemy.y - 9)
      .setScale(enemyScale / 0.205)
      .setDepth(Math.round(this.enemy.y) - 2)
      .setVisible(this.enemyHealth > 0);
    this.enemy.setFlipX(this.player.x > this.enemy.x);
  }

  private drawAttackArc(heavy: boolean, direction: number): void {
    const graphics = this.add.graphics().setDepth(Math.round(this.player.y) + 2);
    graphics.lineStyle(heavy ? 8 : 4, heavy ? 0xe2b966 : 0x9bc5c4, heavy ? 0.82 : 0.72);
    const start = direction > 0 ? -0.78 : Math.PI - 0.78;
    const end = direction > 0 ? 0.72 : Math.PI + 0.72;
    graphics.beginPath();
    graphics.arc(this.player.x + direction * 18, this.player.y - 82, heavy ? 112 : 78, start, end, direction < 0);
    graphics.strokePath();
    this.tweens.add({ targets: graphics, alpha: 0, scaleX: 1.18, scaleY: 1.18, duration: heavy ? 260 : 150, onComplete: () => graphics.destroy() });
  }

  private drawEnemyStrike(): void {
    const graphics = this.add.graphics().setDepth(Math.round(this.enemy.y) + 3);
    graphics.lineStyle(9, 0xb94b3e, 0.72);
    graphics.beginPath();
    graphics.moveTo(this.enemy.x, this.enemy.y - 125);
    graphics.lineTo(this.player.x, this.player.y - 40);
    graphics.strokePath();
    this.tweens.add({ targets: graphics, alpha: 0, duration: 170, onComplete: () => graphics.destroy() });
  }

  private showCallout(text: string, duration = 520): void {
    ui.callout.textContent = text;
    ui.callout.classList.add("visible");
    if (this.calloutTimer) window.clearTimeout(this.calloutTimer);
    this.calloutTimer = window.setTimeout(() => ui.callout.classList.remove("visible"), duration);
  }

  private publish(): void {
    ui.playerHealth.style.width = `${this.playerHealth}%`;
    ui.enemyHealth.style.width = `${this.enemyHealth}%`;
    ui.posture.style.width = `${this.posture}%`;
    ui.stance.textContent = this.stance === "eaves" ? "檐卫势" : "花转势";
    ui.objective.textContent = this.round === "victory" ? "目标完成：伞巷镇守已破" : "目标：击破伞巷镇守";
    ui.actionButtons.forEach((button) => {
      const action = button.dataset.action;
      button.classList.toggle("active", action === "stance" && this.stance === "flower");
    });
    document.body.dataset.round = this.round;
    document.body.dataset.lastAction = this.lastAction;
    document.body.dataset.playerHealth = String(this.playerHealth);
    document.body.dataset.enemyHealth = String(this.enemyHealth);
    document.body.dataset.posture = String(this.posture);
    document.body.dataset.stance = this.stance;
    document.body.dataset.playerX = String(Math.round(this.player.x));
    document.body.dataset.playerY = String(Math.round(this.player.y));
    document.body.dataset.enemyX = String(Math.round(this.enemy.x));
    document.body.dataset.enemyY = String(Math.round(this.enemy.y));
    document.body.dataset.enemyTelegraphing = String(this.enemyTelegraphing);
  }
}

const scene = new DemoScene();

new Phaser.Game({
  type: Phaser.AUTO,
  parent: "game",
  width: 1280,
  height: 720,
  backgroundColor: "#071018",
  transparent: false,
  render: { antialias: true, pixelArt: false, roundPixels: false },
  scale: { mode: Phaser.Scale.NONE, autoCenter: Phaser.Scale.NO_CENTER },
  scene
});

ui.startButton.addEventListener("click", () => scene.startRound());
ui.restartButton.addEventListener("click", () => scene.restartRound());
ui.actionButtons.forEach((button) => {
  button.addEventListener("click", () => scene.performAction(button.dataset.action as ActionName));
});
