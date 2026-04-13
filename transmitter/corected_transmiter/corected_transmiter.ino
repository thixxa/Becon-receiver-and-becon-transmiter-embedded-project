void loop()
{
  // ===== RUN LINE FOLLOW LOGIC =====
  
  int OL = digitalRead(IR_OUT_LEFT);
  int L  = digitalRead(IR_LEFT);
  int M  = digitalRead(IR_MID);
  int R  = digitalRead(IR_RIGHT);
  int OR = digitalRead(IR_OUT_RIGHT);

  // ---- your existing logic ----
  if (L == 0 && M == 1 && R == 0)
  {
    forward(baseSpeed, baseSpeed);
    lastDirection = 0;
  }
  else if (L == 1 && M == 0 && R == 0)
  {
    forward(turnSpeed, baseSpeed);
    lastDirection = -1;
  }
  else if (L == 0 && M == 0 && R == 1)
  {
    forward(baseSpeed, turnSpeed);
    lastDirection = 1;
  }
  else if (L == 1 && M == 1 && R == 0)
  {
    forward(hardTurn, baseSpeed);
    lastDirection = -1;
  }
  else if (L == 0 && M == 1 && R == 1)
  {
    forward(baseSpeed, hardTurn);
    lastDirection = 1;
  }
  else if ((L == 1 && M == 0 && R == 1) || (L == 1 && M == 1 && R == 1))
  {
    forward(baseSpeed, baseSpeed);
  }
  else if (L == 0 && M == 0 && R == 0)
  {
    if (OL == 1)
      forward(searchSpeed, baseSpeed);
    else if (OR == 1)
      forward(baseSpeed, searchSpeed);
    else
    {
      if (lastDirection == -1)
        forward(searchSpeed, baseSpeed);
      else
        forward(baseSpeed, searchSpeed);
    }
  }
  else
  {
    stopMotors();
  }

  // ===== RUN FOR 1 SECOND =====
  delay(1000);
void loop()
{
  // ===== READ SENSOR VALUES =====
  int OL = digitalRead(IR_OUT_LEFT);
  int L  = digitalRead(IR_LEFT);
  int M  = digitalRead(IR_MID);
  int R  = digitalRead(IR_RIGHT);
  int OR = digitalRead(IR_OUT_RIGHT);

  // ===== PRINT SENSOR VALUES =====
  Serial.print("OL: "); Serial.print(OL);
  Serial.print(" L: "); Serial.print(L);
  Serial.print(" M: "); Serial.print(M);
  Serial.print(" R: "); Serial.print(R);
  Serial.print(" OR: "); Serial.println(OR);

  // ===== LINE FOLLOW LOGIC =====
  if (L == 0 && M == 1 && R == 0)
  {
    Serial.println("Forward");
    forward(baseSpeed, baseSpeed);
    lastDirection = 0;
  }
  else if (L == 1 && M == 0 && R == 0)
  {
    Serial.println("Turn Left");
    forward(turnSpeed, baseSpeed);
    lastDirection = -1;
  }
  else if (L == 0 && M == 0 && R == 1)
  {
    Serial.println("Turn Right");
    forward(baseSpeed, turnSpeed);
    lastDirection = 1;
  }
  else if (L == 1 && M == 1 && R == 0)
  {
    Serial.println("Hard Left");
    forward(hardTurn, baseSpeed);
    lastDirection = -1;
  }
  else if (L == 0 && M == 1 && R == 1)
  {
    Serial.println("Hard Right");
    forward(baseSpeed, hardTurn);
    lastDirection = 1;
  }
  else if ((L == 1 && M == 0 && R == 1) || (L == 1 && M == 1 && R == 1))
  {
    Serial.println("Straight (All/Center case)");
    forward(baseSpeed, baseSpeed);
  }
  else if (L == 0 && M == 0 && R == 0)
  {
    Serial.println("Searching...");
    if (OL == 1)
    {
      Serial.println("Search Left");
      forward(searchSpeed, baseSpeed);
    }
    else if (OR == 1)
    {
      Serial.println("Search Right");
      forward(baseSpeed, searchSpeed);
    }
    else
    {
      Serial.println("Search Last Direction");
      if (lastDirection == -1)
        forward(searchSpeed, baseSpeed);
      else
        forward(baseSpeed, searchSpeed);
    }
  }
  else
  {
    Serial.println("Stop");
    stopMotors();
  }

  // ===== RUN FOR 1 SECOND =====
  Serial.println("Running...");
  delay(1000);

  // ===== STOP FOR 0.5 SECOND =====
  Serial.println("Stopped");
  stopMotors();
  delay(500);

  Serial.println("----------------------");
}
  // ===== STOP FOR 0.5 SECOND =====
  stopMotors();
  delay(500);
}