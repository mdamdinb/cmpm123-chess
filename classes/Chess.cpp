#include "Chess.h"
#include <limits>
#include <cmath>
#include <iostream>
#include <fstream>

Chess::Chess()
{
    _grid = new Grid(8, 8);

    std::ofstream logFile("/tmp/chess_debug.log", std::ios::app);
    logFile << "========================================" << std::endl;
    logFile << "CHESS GAME CREATED!" << std::endl;
    logFile << "========================================" << std::endl;
    logFile.close();
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    // Set the gameTag: piece type (1-6) + player color offset (0 for white, 128 for black)
    int gameTag = piece + (playerNumber == 1 ? 128 : 0);
    bit->setGameTag(gameTag);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    startGame();


    std::vector<BitMove> moves;
    generateMoves(moves);

    int totalMoves = moves.size();
}

void Chess::FENtoBoard(const std::string& fen) {
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    // 1: piece placement (from white's perspective)
    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)

    // get just the board part if there's extra stuff after a space
    std::string piecePlacement = fen;
    size_t spacePos = fen.find(' ');
    if (spacePos != std::string::npos) {
        piecePlacement = fen.substr(0, spacePos);
    }

    // clear board before setting up
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->setBit(nullptr);
    });

    // start from top left of board (rank 8, a-file)
    // y=7 is rank 8, y=0 is rank 1
    int x = 0;
    int y = 7;

    for (char c : piecePlacement) {
        if (c == '/') {
            // slash means go to next rank
            y--;
            x = 0;
        } else if (c >= '1' && c <= '8') {
            // numbers mean skip that many squares
            int emptySquares = c - '0';
            x += emptySquares;
        } else {
            ChessPiece pieceType = NoPiece;
            int playerNumber = -1;

            // uppercase = white, lowercase = black
            bool isWhite = (c >= 'A' && c <= 'Z');
            playerNumber = isWhite ? 0 : 1;

            // make it uppercase to check what piece it is
            char pieceChar = isWhite ? c : (c - 'a' + 'A');

            switch (pieceChar) {
                case 'P': pieceType = Pawn; break;
                case 'N': pieceType = Knight; break;
                case 'B': pieceType = Bishop; break;
                case 'R': pieceType = Rook; break;
                case 'Q': pieceType = Queen; break;
                case 'K': pieceType = King; break;
                default: pieceType = NoPiece; break;
            }

            if (pieceType != NoPiece && x >= 0 && x < 8 && y >= 0 && y < 8) {
                Bit* piece = PieceForPlayer(playerNumber, pieceType);
                ChessSquare* square = _grid->getSquare(x, y);
                if (square) {
                    piece->setPosition(square->getPosition());
                    square->setBit(piece);
                }
            }

            x++;
        }
    }
}

void Chess::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    std::ofstream logFile("/tmp/chess_debug.log", std::ios::app);
    logFile << "=== bitMovedFromTo called ===" << std::endl;
    logFile.close();

    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);

    if (dstSquare) {
        Bit* capturedPiece = dstSquare->bit();
        if (capturedPiece && capturedPiece != &bit) {
            delete capturedPiece;
        }
    }

    Game::bitMovedFromTo(bit, src, dst);
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // need to implement friendly/unfriendly in bit so for now this hack
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    int pieceColor = bit.gameTag() & 128;

    if (pieceColor == currentPlayer) {
        return true;
    }
    return false;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    std::ofstream logFile("/tmp/chess_debug.log", std::ios::app);
    logFile << "\n=== canBitMoveFromTo CALLED ===" << std::endl;

    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);

    if (!srcSquare || !dstSquare) {
        logFile << "ERROR: Invalid square cast" << std::endl;
        logFile.close();
        return false;
    }
    int srcX = srcSquare->getColumn();
    int srcY = srcSquare->getRow();
    int dstX = dstSquare->getColumn();
    int dstY = dstSquare->getRow();

    int fromSquare = coordsToSquare(srcX, srcY);
    int toSquare = coordsToSquare(dstX, dstY);

    logFile << "Moving from (" << srcX << "," << srcY << ")[sq" << fromSquare << "]";
    logFile << " to (" << dstX << "," << dstY << ")[sq" << toSquare << "]" << std::endl;
    logFile << "Piece gameTag: " << bit.gameTag() << std::endl;

    std::vector<BitMove> legalMoves;
    generateMoves(legalMoves);

    logFile << "Generated " << legalMoves.size() << " legal moves total" << std::endl;

    ChessPiece pieceType = getPieceType(bit.gameTag());
    logFile << "Piece type: " << pieceType << " (1=Pawn, 2=Knight, 6=King)" << std::endl;

    // Log all legal moves for this piece
    logFile << "Legal moves for this piece type:" << std::endl;
    int count = 0;
    for (const auto& move : legalMoves) {
        if (move.piece == pieceType && move.from == fromSquare) {
            logFile << "  Can move to square " << (int)move.to << std::endl;
            count++;
        }
    }
    logFile << "Total legal moves for this piece: " << count << std::endl;

    for (const auto& move : legalMoves) {
        if (move.from == fromSquare && move.to == toSquare && move.piece == pieceType) {
            logFile << "✓ LEGAL MOVE!" << std::endl;
            logFile.close();
            return true;
        }
    }

    logFile << "✗ ILLEGAL MOVE" << std::endl;
    logFile.close();
    return false;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
            s += pieceNotation( x, y );
        }
    );
    return s;}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}

ChessPiece Chess::getPieceType(int gameTag) const
{
    int pieceValue = gameTag & 0x7; // Get the lower 3 bits
    return static_cast<ChessPiece>(pieceValue);
}

bool Chess::isSquareOccupied(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return false;
    auto square = _grid->getSquare(x, y);
    return square && square->bit() != nullptr;
}

bool Chess::isSquareOccupiedByEnemy(int x, int y, bool isWhite) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return false;
    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) return false;

    bool targetIsWhite = isWhitePiece(square->bit()->gameTag());
    return targetIsWhite != isWhite;
}

// Move generation
void Chess::generateMoves(std::vector<BitMove>& moves)
{
    moves.clear();

    int currentPlayerNum = getCurrentPlayer()->playerNumber();
    bool isWhite = (currentPlayerNum == 0);

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            auto square = _grid->getSquare(x, y);
            if (!square || !square->bit()) continue;

            Bit* piece = square->bit();
            int gameTag = piece->gameTag();
            bool pieceIsWhite = isWhitePiece(gameTag);
            if (pieceIsWhite != isWhite) continue;

            int squareIndex = coordsToSquare(x, y);
            ChessPiece pieceType = getPieceType(gameTag);

            switch (pieceType) {
                case Pawn:
                    generatePawnMoves(squareIndex, moves);
                    break;
                case Knight:
                    generateKnightMoves(squareIndex, moves);
                    break;
                case Bishop:
                    generateBishopMoves(squareIndex, moves);
                    break;
                case Rook:
                    generateRookMoves(squareIndex, moves);
                    break;
                case Queen:
                    generateQueenMoves(squareIndex, moves);
                    break;
                case King:
                    generateKingMoves(squareIndex, moves);
                    break;
                default:
                    break;
            }
        }
    }
}

void Chess::generatePawnMoves(int square, std::vector<BitMove>& moves)
{
    int x, y;
    squareToCoords(square, x, y);

    auto sourceSq = _grid->getSquare(x, y);
    if (!sourceSq || !sourceSq->bit()) return;

    bool isWhite = isWhitePiece(sourceSq->bit()->gameTag());
    int direction = isWhite ? 1 : -1;
    int startRank = isWhite ? 1 : 6;

    // Forward one square
    int newY = y + direction;
    if (newY >= 0 && newY < 8 && !isSquareOccupied(x, newY)) {
        moves.push_back(BitMove(square, coordsToSquare(x, newY), Pawn));

        // Double forward move from starting position
        if (y == startRank) {
            int doubleY = y + (direction * 2);
            if (!isSquareOccupied(x, doubleY)) {
                moves.push_back(BitMove(square, coordsToSquare(x, doubleY), Pawn));
            }
        }
    }

    // Diagonal captures
    for (int dx = -1; dx <= 1; dx += 2) {
        int captureX = x + dx;
        int captureY = y + direction;

        if (captureX >= 0 && captureX < 8 && captureY >= 0 && captureY < 8) {
            if (isSquareOccupiedByEnemy(captureX, captureY, isWhite)) {
                moves.push_back(BitMove(square, coordsToSquare(captureX, captureY), Pawn));
            }
        }
    }
}

void Chess::generateKnightMoves(int square, std::vector<BitMove>& moves)
{
    int x, y;
    squareToCoords(square, x, y);

    auto sourceSq = _grid->getSquare(x, y);
    if (!sourceSq || !sourceSq->bit()) return;

    bool isWhite = isWhitePiece(sourceSq->bit()->gameTag());

    uint64_t attacks = KnightAttacks[square];

    for (int targetSquare = 0; targetSquare < 64; targetSquare++) {
        if (attacks & (1ULL << targetSquare)) {
            int targetX, targetY;
            squareToCoords(targetSquare, targetX, targetY);
            if (!isSquareOccupied(targetX, targetY) ||
                isSquareOccupiedByEnemy(targetX, targetY, isWhite)) {
                moves.push_back(BitMove(square, targetSquare, Knight));
            }
        }
    }
}

void Chess::generateKingMoves(int square, std::vector<BitMove>& moves)
{
    int x, y;
    squareToCoords(square, x, y);

    auto sourceSq = _grid->getSquare(x, y);
    if (!sourceSq || !sourceSq->bit()) return;

    bool isWhite = isWhitePiece(sourceSq->bit()->gameTag());

    uint64_t attacks = KingAttacks[square];

    for (int targetSquare = 0; targetSquare < 64; targetSquare++) {
        if (attacks & (1ULL << targetSquare)) {
            int targetX, targetY;
            squareToCoords(targetSquare, targetX, targetY);
            if (!isSquareOccupied(targetX, targetY) ||
                isSquareOccupiedByEnemy(targetX, targetY, isWhite)) {
                moves.push_back(BitMove(square, targetSquare, King));
            }
        }
    }
}

void Chess::generateRookMoves(int square, std::vector<BitMove>& moves)
{
    int x, y;
    squareToCoords(square, x, y);

    auto sourceSq = _grid->getSquare(x, y);
    if (!sourceSq || !sourceSq->bit()) return;

    bool isWhite = isWhitePiece(sourceSq->bit()->gameTag());

    int directions[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};

    for (int dir = 0; dir < 4; dir++) {
        int dx = directions[dir][0];
        int dy = directions[dir][1];

        for (int dist = 1; dist < 8; dist++) {
            int targetX = x + (dx * dist);
            int targetY = y + (dy * dist);

            if (targetX < 0 || targetX >= 8 || targetY < 0 || targetY >= 8) break;

            if (isSquareOccupied(targetX, targetY)) {
                if (isSquareOccupiedByEnemy(targetX, targetY, isWhite)) {
                    moves.push_back(BitMove(square, coordsToSquare(targetX, targetY), Rook));
                }
                break;
            }

            moves.push_back(BitMove(square, coordsToSquare(targetX, targetY), Rook));
        }
    }
}

void Chess::generateBishopMoves(int square, std::vector<BitMove>& moves)
{
    int x, y;
    squareToCoords(square, x, y);

    auto sourceSq = _grid->getSquare(x, y);
    if (!sourceSq || !sourceSq->bit()) return;

    bool isWhite = isWhitePiece(sourceSq->bit()->gameTag());

    int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    for (int dir = 0; dir < 4; dir++) {
        int dx = directions[dir][0];
        int dy = directions[dir][1];

        for (int dist = 1; dist < 8; dist++) {
            int targetX = x + (dx * dist);
            int targetY = y + (dy * dist);

            if (targetX < 0 || targetX >= 8 || targetY < 0 || targetY >= 8) break;

            if (isSquareOccupied(targetX, targetY)) {
                if (isSquareOccupiedByEnemy(targetX, targetY, isWhite)) {
                    moves.push_back(BitMove(square, coordsToSquare(targetX, targetY), Bishop));
                }
                break;
            }

            moves.push_back(BitMove(square, coordsToSquare(targetX, targetY), Bishop));
        }
    }
}

void Chess::generateQueenMoves(int square, std::vector<BitMove>& moves)
{
    generateRookMoves(square, moves);
    generateBishopMoves(square, moves);

    for (auto& move : moves) {
        if (move.from == square && (move.piece == Rook || move.piece == Bishop)) {
            move.piece = Queen;
        }
    }
}
