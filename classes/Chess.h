#pragma once

#include "Game.h"
#include "Grid.h"
#include "BitMove.h"
#include "MagicBitboards.h"
#include <vector>

constexpr int pieceSize = 80;

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    void bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;

    void generateMoves(std::vector<BitMove>& moves);
    void generatePawnMoves(int square, std::vector<BitMove>& moves);
    void generateKnightMoves(int square, std::vector<BitMove>& moves);
    void generateKingMoves(int square, std::vector<BitMove>& moves);
    int coordsToSquare(int x, int y) const { return y * 8 + x; }
    void squareToCoords(int square, int& x, int& y) const { x = square % 8; y = square / 8; }
    ChessPiece getPieceType(int gameTag) const;
    bool isWhitePiece(int gameTag) const { return gameTag < 128; }
    bool isSquareOccupied(int x, int y) const;
    bool isSquareOccupiedByEnemy(int x, int y, bool isWhite) const;

    Grid* _grid;
};