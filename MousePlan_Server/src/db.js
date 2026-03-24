const fs = require("fs");
const path = require("path");

function ensureDir(filePath) {
  const dir = path.dirname(filePath);
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
}

function initialData() {
  return {
    users: {},
    plans: {},
    records: {},
    registrationCodes: {},
  };
}

function loadDb(dbPath) {
  ensureDir(dbPath);
  if (!fs.existsSync(dbPath)) {
    const seed = initialData();
    fs.writeFileSync(dbPath, JSON.stringify(seed, null, 2), "utf8");
    return seed;
  }

  const raw = fs.readFileSync(dbPath, "utf8").trim();
  if (!raw) {
    return initialData();
  }

  try {
    const parsed = JSON.parse(raw);
    return {
      users: parsed.users || {},
      plans: parsed.plans || {},
      records: parsed.records || {},
      registrationCodes: parsed.registrationCodes || {},
    };
  } catch {
    const fallback = initialData();
    fs.writeFileSync(dbPath, JSON.stringify(fallback, null, 2), "utf8");
    return fallback;
  }
}

function saveDb(dbPath, db) {
  ensureDir(dbPath);
  fs.writeFileSync(dbPath, JSON.stringify(db, null, 2), "utf8");
}

function upsertPlan(db, userId, plan) {
  if (!db.plans[userId]) {
    db.plans[userId] = [];
  }
  const list = db.plans[userId];
  const idx = list.findIndex((p) => p && p.id === plan.id);
  if (idx >= 0) {
    list[idx] = plan;
  } else {
    list.push(plan);
  }
}

function upsertRecord(db, userId, record) {
  if (!db.records[userId]) {
    db.records[userId] = [];
  }
  const list = db.records[userId];
  const idx = list.findIndex((r) => r && r.date === record.date);
  if (idx >= 0) {
    list[idx] = record;
  } else {
    list.push(record);
  }
}

module.exports = {
  loadDb,
  saveDb,
  upsertPlan,
  upsertRecord,
};
