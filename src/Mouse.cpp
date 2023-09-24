#pragma once
#include "stdafx.h"
#include "Mouse.h"

/*
 * Mouse::Mouse
 */
Mouse::Mouse() : m_xPosition(0), m_yPosition(0), m_leftButtonPressed(0),
	m_rightButtonPressed(false), m_inWindow(false), m_wheelValue(0){/*empty*/}


/*
 * Mouse::GetPos
 */
std::pair<int, int> Mouse::GetPos() {
	return { m_xPosition, m_yPosition };
}


/*
 * Mouse::GetPosX
 */
int Mouse::GetPosX() {
	return m_xPosition;
}


/*
 * Mouse::GetPosY
 */
int Mouse::GetPosY() {
	return m_yPosition;
}


/*
 * Mouse::GetWheelValue
 */
int Mouse::GetWheelValue() {
	return m_wheelValue / WHEEL_DELTA;
}


/*
 * Mouse::IsInWindow
 */
bool Mouse::IsInWindow() {
	return m_inWindow;
}


/*
 * Mouse::LeftIsPressed
 */
bool Mouse::LeftIsPressed() {
	return m_leftButtonPressed;
}


/*
 * Mouse::RightIsPressed
 */
bool Mouse::RightIsPressed() {
	return m_rightButtonPressed;
}


/*
 * Mouse::OnMouseMove
 */
void Mouse::OnMouseMove(int x, int y) {
	m_xPosition = x;
	m_yPosition = y;
}


/*
 * Mouse::OnMouseLeave
 */
void Mouse::OnMouseLeave() {
	m_inWindow = false;
}


/*
 * Mouse::OnMouseEnter
 */
void Mouse::OnMouseEnter() {
	m_inWindow = true;
}


/*
 * Mouse::OnLeftPressed
 */
void Mouse::OnLeftPressed() {
	m_leftButtonPressed = true;
}


/*
 * Mouse::OnLeftReleased
 */
void Mouse::OnLeftReleased() {
	m_leftButtonPressed = false;
}


/*
 * Mouse::OnRightPressed
 */
void Mouse::OnRightPressed() {
	m_rightButtonPressed = true;
}


/*
 * Mouse::OnRightReleased
 */
void Mouse::OnRightReleased() {
	m_rightButtonPressed = false;
}


/*
 * Mouse::OnWheelValue
 */
void Mouse::OnWheelValue(int delta) {
	m_wheelValue += delta;
}


/*
 * Mouse::ResetWheelValue
 */
void Mouse::ResetWheelValue() {
	m_wheelValue = 0;
}