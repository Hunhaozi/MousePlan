require("dotenv").config();

const fs = require("fs");
const path = require("path");
const express = require("express");
const cors = require("cors");
const morgan = require("morgan");

const config = require("./config");
const { loadDb, saveDb, upsertPlan, upsertRecord } = require("./db");

const app = express();

app.use(cors());
app.use(express.json({ limit: "2mb" }));
app.use(morgan("dev"));

let db = loadDb(config.dbPath);

function ok(res, extra = {}) {
  res.json({ success: true, ...extra });
}

function fail(res, message, status = 400, extra = {}) {
  res.status(status).json({ success: false, message, ...extra });
}

function getUserById(userId) {
  if (!userId) {
    return null;
  }
  return db.users[userId] || null;
}

function findUserByUsername(username) {
  if (!username) {
    return null;
  }
  return Object.values(db.users).find((u) => u && u.username === username) || null;
}

function buildPackage1(userId) {
  const userEntry = db.users[userId];
  if (!userEntry) {
    return null;
  }

  const userProfile = userEntry.profile && typeof userEntry.profile === "object"
    ? userEntry.profile
    : {
        id: userEntry.id,
        username: userEntry.username || "",
      };

  return {
    schema: "mouseplan.package1.v1",
    savedAt: new Date().toISOString(),
    userId,
    user: userProfile,
    plans: Array.isArray(db.plans[userId]) ? db.plans[userId] : [],
    records: Array.isArray(db.records[userId]) ? db.records[userId] : [],
  };
}

function makeAbsoluteUrl(req, path) {
  const host = req.get("host");
  const protoHeader = req.get("x-forwarded-proto");
  const proto = protoHeader ? protoHeader.split(",")[0].trim() : req.protocol;
  if (!host) {
    return path;
  }
  return `${proto}://${host}${path}`;
}

if (!fs.existsSync(config.appUpdatePackageDir)) {
  fs.mkdirSync(config.appUpdatePackageDir, { recursive: true });
}

if (!fs.existsSync(config.feedbackStorageDir)) {
  fs.mkdirSync(config.feedbackStorageDir, { recursive: true });
}
if (!fs.existsSync(config.feedbackStorageFilePath)) {
  fs.writeFileSync(config.feedbackStorageFilePath, JSON.stringify([], null, 2), "utf8");
}

app.use("/downloads", express.static(config.appUpdatePackageDir));

app.get("/health", (_req, res) => {
  ok(res, { service: "mouseplan-server", time: new Date().toISOString() });
});

app.get("/app/update/latest", (req, res) => {
  const latestVersion = config.appLatestVersion || "1.00";
  const apkPath = `/downloads/${encodeURIComponent(config.appUpdatePackageName)}`;
  const fullApkFilePath = path.join(config.appUpdatePackageDir, config.appUpdatePackageName);
  const packageExists = fs.existsSync(fullApkFilePath);

  return ok(res, {
    latestVersion,
    changelog: config.appUpdateChangelog,
    apkPath,
    apkUrl: makeAbsoluteUrl(req, apkPath),
    packageExists,
    packageName: config.appUpdatePackageName,
    packageDir: config.appUpdatePackageDir,
  });
});

app.post("/app/feedback/submit", (req, res) => {
  const { userId, username, content, submittedAt } = req.body || {};
  if (!userId || !content) {
    return fail(res, "userId and content are required", 400);
  }

  const normalized = String(content).trim();
  if (!normalized) {
    return fail(res, "content cannot be empty", 400);
  }

  let feedbackList = [];
  try {
    const raw = fs.readFileSync(config.feedbackStorageFilePath, "utf8");
    const parsed = JSON.parse(raw);
    if (Array.isArray(parsed)) {
      feedbackList = parsed;
    }
  } catch (_err) {
    feedbackList = [];
  }

  feedbackList.push({
    userId: String(userId),
    username: String(username || ""),
    content: normalized,
    submittedAt: submittedAt || new Date().toISOString(),
    serverReceivedAt: new Date().toISOString(),
  });

  fs.writeFileSync(config.feedbackStorageFilePath, JSON.stringify(feedbackList, null, 2), "utf8");
  return ok(res);
});

app.post("/auth/verify-registration-code", (req, res) => {
  const { codeHash } = req.body || {};
  if (!codeHash || typeof codeHash !== "string") {
    return fail(res, "codeHash is required", 400, { valid: false });
  }

  const key = codeHash.trim();
  const entry = db.registrationCodes[key];
  if (entry) {
    return ok(res, { valid: !entry.used });
  }

  const validFromEnv = config.allowedCodeHashes.includes(key);
  const valid = config.strictRegCode ? validFromEnv : false;
  return ok(res, { valid });
});

app.post("/auth/consume-registration-code", (req, res) => {
  const { codeHash, userId } = req.body || {};
  if (!codeHash || !userId) {
    return fail(res, "codeHash and userId are required", 400);
  }

  const key = String(codeHash).trim();
  const entry = db.registrationCodes[key];
  if (!entry) {
    if (config.allowedCodeHashes.includes(key)) {
      db.registrationCodes[key] = {
        used: true,
        usedByUserId: userId,
        createdAt: new Date().toISOString(),
        consumedAt: new Date().toISOString(),
      };
      saveDb(config.dbPath, db);
      return ok(res);
    }
    return ok(res, { success: false, message: "registration code not found" });
  }

  if (entry.used) {
    return ok(res, { success: false, message: "registration code already used" });
  }

  db.registrationCodes[key] = {
    ...entry,
    used: true,
    usedByUserId: userId,
    consumedAt: new Date().toISOString(),
  };
  saveDb(config.dbPath, db);
  return ok(res);
});

app.post("/admin/registration-code/add", (req, res) => {
  return fail(res,
              "registration code can only be added by local server script",
              403);
});

app.post("/auth/upload-password-hash", (req, res) => {
  const { userId, passwordHash } = req.body || {};
  if (!userId || !passwordHash) {
    return fail(res, "userId and passwordHash are required", 400);
  }

  const existing = getUserById(userId) || {
    id: userId,
    username: "",
    profile: {},
  };

  db.users[userId] = {
    ...existing,
    id: userId,
    passwordHash,
    updatedAt: new Date().toISOString(),
  };
  saveDb(config.dbPath, db);
  return ok(res);
});

app.post("/auth/login", (req, res) => {
  const { userId, username, passwordHash } = req.body || {};
  if (!passwordHash) {
    return fail(res, "passwordHash is required", 400);
  }

  let user = getUserById(userId);
  if (!user && username) {
    user = findUserByUsername(username);
  }

  if (!user && config.allowLoginAutoProvision && username) {
    const newId = userId || `u_${Date.now()}`;
    user = {
      id: newId,
      username,
      profile: {},
      passwordHash,
      updatedAt: new Date().toISOString(),
    };
    db.users[newId] = user;
    saveDb(config.dbPath, db);
    return ok(res, { userId: newId });
  }

  if (!user) {
    return ok(res, { success: false, message: "user not found" });
  }

  const pass = user.passwordHash === passwordHash;
  if (!pass) {
    return ok(res, { success: false, message: "invalid password" });
  }

  if (!user.id && userId) {
    user.id = userId;
  }

  return ok(res, { userId: user.id });
});

app.post("/sync/user", (req, res) => {
  const { user } = req.body || {};
  if (!user || typeof user !== "object") {
    return fail(res, "user object is required", 400);
  }
  if (!user.id) {
    return fail(res, "user.id is required", 400);
  }

  const existing = getUserById(user.id) || {};
  db.users[user.id] = {
    ...existing,
    id: user.id,
    username: user.username || existing.username || "",
    profile: user,
    updatedAt: new Date().toISOString(),
  };

  saveDb(config.dbPath, db);
  return ok(res);
});

app.post("/sync/plan", (req, res) => {
  const { userId, plan } = req.body || {};
  if (!userId || !plan || typeof plan !== "object") {
    return fail(res, "userId and plan are required", 400);
  }

  upsertPlan(db, userId, {
    ...plan,
    syncedAt: new Date().toISOString(),
  });
  saveDb(config.dbPath, db);
  return ok(res);
});

app.post("/sync/record", (req, res) => {
  const { userId, record } = req.body || {};
  if (!userId || !record || typeof record !== "object") {
    return fail(res, "userId and record are required", 400);
  }

  upsertRecord(db, userId, {
    ...record,
    syncedAt: new Date().toISOString(),
  });
  saveDb(config.dbPath, db);
  return ok(res);
});

app.post("/sync/package1/push", (req, res) => {
  const { userId, package1 } = req.body || {};
  if (!userId || !package1 || typeof package1 !== "object") {
    return fail(res, "userId and package1 are required", 400);
  }

  const pkgUser = package1.user && typeof package1.user === "object" ? package1.user : null;
  const existing = getUserById(userId) || { id: userId, username: "", profile: {} };

  db.users[userId] = {
    ...existing,
    id: userId,
    username: (pkgUser && pkgUser.username) || existing.username || "",
    profile: pkgUser || existing.profile || {},
    updatedAt: new Date().toISOString(),
  };

  if (Array.isArray(package1.plans)) {
    db.plans[userId] = package1.plans;
  }
  if (Array.isArray(package1.records)) {
    db.records[userId] = package1.records;
  }

  saveDb(config.dbPath, db);
  return ok(res);
});

app.post("/sync/package1/pull", (req, res) => {
  const { userId } = req.body || {};
  if (!userId) {
    return fail(res, "userId is required", 400);
  }

  const package1 = buildPackage1(userId);
  if (!package1) {
    return ok(res, { success: false, message: "user data not found" });
  }
  return ok(res, { package1 });
});

app.use((err, _req, res, _next) => {
  const message = err && err.message ? err.message : "unknown server error";
  return fail(res, message, 500);
});

app.listen(config.port, () => {
  // eslint-disable-next-line no-console
  console.log(`[mouseplan-server] listening on :${config.port}`);
  // eslint-disable-next-line no-console
  console.log(`[mouseplan-server] db file: ${config.dbPath}`);
  // eslint-disable-next-line no-console
  console.log(`[mouseplan-server] strict reg code: ${config.strictRegCode}`);
});
