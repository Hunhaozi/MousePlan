const path = require("path");

function asBool(value, defaultValue = false) {
  if (value === undefined || value === null || value === "") {
    return defaultValue;
  }
  const normalized = String(value).trim().toLowerCase();
  return normalized === "1" || normalized === "true" || normalized === "yes";
}

function parseHashList(value) {
  if (!value) {
    return [];
  }
  return String(value)
    .split(",")
    .map((s) => s.trim())
    .filter(Boolean);
}

module.exports = {
  port: Number(process.env.PORT || 8080),
  dbPath: path.resolve(__dirname, "..", "data", "db.json"),
  strictRegCode: asBool(process.env.STRICT_REG_CODE, false),
  allowedCodeHashes: parseHashList(process.env.ALLOWED_CODE_HASHES),
  allowLoginAutoProvision: asBool(process.env.ALLOW_LOGIN_AUTO_PROVISION, false),
  appLatestVersion: String(process.env.APP_LATEST_VERSION || "1.00").trim(),
  appUpdateChangelog: String(process.env.APP_UPDATE_CHANGELOG || "").trim(),
  appUpdatePackageDir: path.resolve(
    __dirname,
    "..",
    process.env.APP_UPDATE_PACKAGE_DIR || path.join("data", "updates")
  ),
  appUpdatePackageName: String(process.env.APP_UPDATE_PACKAGE_NAME || "MousePlan_latest.apk").trim(),
  feedbackStorageDir: path.resolve(
    __dirname,
    "..",
    process.env.FEEDBACK_STORAGE_DIR || path.join("data", "feedback")
  ),
  feedbackStorageFilePath: path.resolve(
    __dirname,
    "..",
    process.env.FEEDBACK_STORAGE_FILE || path.join("data", "feedback", "suggestions.json")
  ),
};
