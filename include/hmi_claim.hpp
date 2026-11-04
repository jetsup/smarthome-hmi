#pragma once
#include <Arduino.h>

// Pairing from the app or website: this display asks the hub for a short-lived
// code, shows it (and as a QR code), and the owner confirms it from their
// account. The display then collects its token.
struct ClaimCode {
  bool ok = false;
  String code;
  String secret;
  int expiresIn = 0;
  String error;
};

enum class ClaimState { Pending, Claimed, Expired, Error };

struct ClaimResult {
  ClaimState state = ClaimState::Error;
  String token, gatewayId, gatewayName;
};

ClaimCode hmiRequestClaimCode();
ClaimResult hmiPollClaim(const String& secret);
