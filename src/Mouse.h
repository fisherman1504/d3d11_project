#pragma once

/// <summary>
/// This class encapsulates all mouse input mechanisms. TODO: Remove in the future.
/// </summary>
class Mouse{
public:
	/// <summary>
	/// Constructor.
	/// </summary>
	Mouse();

	/// <summary>
	/// Returns mouse cursor position.
	/// </summary>
	/// <returns></returns>
	std::pair<int, int> GetPos();

	/// <summary>
	/// Returns just x component of mouse cursor position.
	/// </summary>
	/// <returns></returns>
	int GetPosX();

	/// <summary>
	/// Returns just y component of mouse cursor position.
	/// </summary>
	/// <returns></returns>
	int GetPosY();

	/// <summary>
	/// Gets the current wheel value.
	/// </summary>
	/// <remarks>
	/// Step size 1. Default: 0.
	/// </remarks>
	/// <returns></returns>
	int GetWheelValue();

	/// <summary>
	/// Returns whether cursor is currently in window or not.
	/// </summary>
	/// <returns></returns>
	bool IsInWindow();

	/// <summary>
	/// Returns whether left mouse button is pressed.
	/// </summary>
	/// <returns></returns>
	bool LeftIsPressed();

	/// <summary>
	/// Update mouse position.
	/// </summary>
	/// <param name="x"></param>
	/// <param name="y"></param>
	void OnMouseMove(int x, int y);

	/// <summary>
	/// Update that mouse cursor left window.
	/// </summary>
	void OnMouseLeave();

	/// <summary>
	/// Update that mouse cursor entered window.
	/// </summary>
	void OnMouseEnter();

	/// <summary>
	/// Update that left mouse button is currently being pressed.
	/// </summary>
	/// <param name="x"></param>
	/// <param name="y"></param>
	void OnLeftPressed();

	/// <summary>
	/// Update that left mouse button is no longer pressed.
	/// </summary>
	/// <param name="x"></param>
	/// <param name="y"></param>
	void OnLeftReleased();

	/// <summary>
	/// Update that right mouse button is currently being pressed.
	/// </summary>
	void OnRightPressed();

	/// <summary>
	/// Update that right mouse button is no longer pressed.
	/// </summary>
	/// <param name="x"></param>
	/// <param name="y"></param>
	void OnRightReleased();

	/// <summary>
	/// Update wheel value.
	/// </summary>
	/// <param name="x"></param>
	/// <param name="y"></param>
	/// <param name="delta"></param>
	void OnWheelValue(int delta);

	/// <summary>
	/// Resets the wheel value to zero.
	/// </summary>
	void ResetWheelValue();

	/// <summary>
	/// Returns whether right mouse button is pressed.
	/// </summary>
	/// <returns></returns>
	bool RightIsPressed();

	// Mouse state.
	bool m_inWindow;			// Whether cursor is in application window or not.
	bool m_leftButtonPressed;	// Left mouse button pressed.
	bool m_rightButtonPressed;	// Right mouse button pressed.
	int m_wheelValue;			// State of mouse wheel. Can be reset to 0.
	int m_xPosition;			// Position of cursor in x dimension.
	int m_yPosition;			// Position of cursor in y dimension.
};