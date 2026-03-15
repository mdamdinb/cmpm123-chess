#include "Chess.h"
#include "Evaluate.h"
#include <limits>
#include <cmath>
#include <iostream>
#include <fstream>

Chess::Chess()
{
    _grid = new Grid(8, 8);

    // Initialize castling flags
    _whiteKingMoved = false;
    _blackKingMoved = false;
    _whiteKingsideRookMoved = false;
    _whiteQueensideRookMoved = false;
    _blackKingsideRookMoved = false;
    _blackQueensideRookMoved = false;

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

    setAIPlayer(1);

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

    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);

    if (srcSquare && dstSquare) {
        int srcX = srcSquare->getColumn();
        int srcY = srcSquare->getRow();
        int dstX = dstSquare->getColumn();
        int dstY = dstSquare->getRow();

        ChessPiece pieceType = getPieceType(bit.gameTag());
        bool isWhite = isWhitePiece(bit.gameTag());

        // Track castling rights
        if (pieceType == King) {
            logFile << "King moved, updating castling rights" << std::endl;
            if (isWhite) {
                _whiteKingMoved = true;

                // Handle castling move
                if (srcX == 4 && dstX == 6 && srcY == 0) {
                    // White kingside castling
                    logFile << "White kingside castling!" << std::endl;
                    ChessSquare* rookFrom = _grid->getSquare(7, 0);
                    ChessSquare* rookTo = _grid->getSquare(5, 0);
                    if (rookFrom && rookTo && rookFrom->bit()) {
                        Bit* rook = rookFrom->bit();
                        rook->setPosition(rookTo->getPosition());
                        rookFrom->setBitWithoutDelete(nullptr);
                        rookTo->setBitWithoutDelete(rook);
                    }
                } else if (srcX == 4 && dstX == 2 && srcY == 0) {
                    // White queenside castling
                    logFile << "White queenside castling!" << std::endl;
                    ChessSquare* rookFrom = _grid->getSquare(0, 0);
                    ChessSquare* rookTo = _grid->getSquare(3, 0);
                    if (rookFrom && rookTo && rookFrom->bit()) {
                        Bit* rook = rookFrom->bit();
                        rook->setPosition(rookTo->getPosition());
                        rookFrom->setBitWithoutDelete(nullptr);
                        rookTo->setBitWithoutDelete(rook);
                    }
                }
            } else {
                _blackKingMoved = true;

                // Handle castling move
                if (srcX == 4 && dstX == 6 && srcY == 7) {
                    // Black kingside castling
                    logFile << "Black kingside castling!" << std::endl;
                    ChessSquare* rookFrom = _grid->getSquare(7, 7);
                    ChessSquare* rookTo = _grid->getSquare(5, 7);
                    if (rookFrom && rookTo && rookFrom->bit()) {
                        Bit* rook = rookFrom->bit();
                        rook->setPosition(rookTo->getPosition());
                        rookFrom->setBitWithoutDelete(nullptr);
                        rookTo->setBitWithoutDelete(rook);
                    }
                } else if (srcX == 4 && dstX == 2 && srcY == 7) {
                    // Black queenside castling
                    logFile << "Black queenside castling!" << std::endl;
                    ChessSquare* rookFrom = _grid->getSquare(0, 7);
                    ChessSquare* rookTo = _grid->getSquare(3, 7);
                    if (rookFrom && rookTo && rookFrom->bit()) {
                        Bit* rook = rookFrom->bit();
                        rook->setPosition(rookTo->getPosition());
                        rookFrom->setBitWithoutDelete(nullptr);
                        rookTo->setBitWithoutDelete(rook);
                    }
                }
            }
        } else if (pieceType == Rook) {
            logFile << "Rook moved from (" << srcX << "," << srcY << ")" << std::endl;
            if (isWhite) {
                if (srcX == 7 && srcY == 0) {
                    _whiteKingsideRookMoved = true;
                    logFile << "White kingside rook moved" << std::endl;
                }
                if (srcX == 0 && srcY == 0) {
                    _whiteQueensideRookMoved = true;
                    logFile << "White queenside rook moved" << std::endl;
                }
            } else {
                if (srcX == 7 && srcY == 7) {
                    _blackKingsideRookMoved = true;
                    logFile << "Black kingside rook moved" << std::endl;
                }
                if (srcX == 0 && srcY == 7) {
                    _blackQueensideRookMoved = true;
                    logFile << "Black queenside rook moved" << std::endl;
                }
            }
        }

        // Handle captured piece
        Bit* capturedPiece = dstSquare->bit();
        if (capturedPiece && capturedPiece != &bit) {
            delete capturedPiece;
        }
    }

    logFile.close();

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
        char piece = s[index];

        if (piece == '0') {
            square->setBitWithoutDelete(nullptr);
        } else {
            bool isWhite = (piece >= 'A' && piece <= 'Z');
            char pieceChar = isWhite ? piece : (piece - 'a' + 'A');
            ChessPiece pieceType = NoPiece;

            switch (pieceChar) {
                case 'P': pieceType = Pawn; break;
                case 'N': pieceType = Knight; break;
                case 'B': pieceType = Bishop; break;
                case 'R': pieceType = Rook; break;
                case 'Q': pieceType = Queen; break;
                case 'K': pieceType = King; break;
                default: pieceType = NoPiece; break;
            }

            if (pieceType != NoPiece) {
                int playerNumber = isWhite ? 0 : 1;
                Bit* newBit = PieceForPlayer(playerNumber, pieceType);
                square->setBitWithoutDelete(newBit);
                newBit->setPosition(square->getPosition());
            } else {
                square->setBitWithoutDelete(nullptr);
            }
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

void Chess::generateMovesFromString(const std::string& state, std::vector<BitMove>& moves, bool isWhite)
{
    // Store all current pieces to restore them later
    std::vector<std::pair<ChessSquare*, Bit*>> savedPieces;
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        savedPieces.push_back({square, square->bit()});
    });

    // Temporarily set board from string
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char piece = state[index];

        if (piece == '0') {
            square->setBitWithoutDelete(nullptr);
        } else {
            bool pieceIsWhite = (piece >= 'A' && piece <= 'Z');
            char pieceChar = pieceIsWhite ? piece : (piece - 'a' + 'A');
            ChessPiece pieceType = NoPiece;

            switch (pieceChar) {
                case 'P': pieceType = Pawn; break;
                case 'N': pieceType = Knight; break;
                case 'B': pieceType = Bishop; break;
                case 'R': pieceType = Rook; break;
                case 'Q': pieceType = Queen; break;
                case 'K': pieceType = King; break;
            }

            if (pieceType != NoPiece) {
                Bit* tempBit = new Bit();
                tempBit->setGameTag(pieceType + (pieceIsWhite ? 0 : 128));
                square->setBitWithoutDelete(tempBit);
            } else {
                square->setBitWithoutDelete(nullptr);
            }
        }
    });

    // Save and restore current turn
    int oldTurn = _gameOptions.currentTurnNo;
    _gameOptions.currentTurnNo = isWhite ? 0 : 1;

    // Generate moves
    generateMoves(moves);

    // Restore turn
    _gameOptions.currentTurnNo = oldTurn;

    // Delete temp pieces and restore original board
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        Bit* tempBit = square->bit();
        if (tempBit) {
            // Check if this was a newly created temp bit
            bool isOriginal = false;
            for (auto& saved : savedPieces) {
                if (saved.second == tempBit) {
                    isOriginal = true;
                    break;
                }
            }
            if (!isOriginal) {
                delete tempBit;
            }
        }
    });

    // Restore original pieces
    for (auto& saved : savedPieces) {
        saved.first->setBitWithoutDelete(saved.second);
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

    // Add castling moves
    generateCastlingMoves(square, moves);
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

void Chess::generateCastlingMoves(int square, std::vector<BitMove>& moves)
{
    int x, y;
    squareToCoords(square, x, y);

    auto sourceSq = _grid->getSquare(x, y);
    if (!sourceSq || !sourceSq->bit()) return;

    bool isWhite = isWhitePiece(sourceSq->bit()->gameTag());

    // White castling
    if (isWhite && !_whiteKingMoved && y == 0 && x == 4) {
        // Kingside castling (e1 to g1)
        if (!_whiteKingsideRookMoved) {
            // Check squares between king and rook are empty
            if (!isSquareOccupied(5, 0) && !isSquareOccupied(6, 0)) {
                // Add castling move (king moves 2 squares to the right)
                moves.push_back(BitMove(square, coordsToSquare(6, 0), King));
            }
        }
        // Queenside castling (e1 to c1)
        if (!_whiteQueensideRookMoved) {
            // Check squares between king and rook are empty
            if (!isSquareOccupied(1, 0) && !isSquareOccupied(2, 0) && !isSquareOccupied(3, 0)) {
                // Add castling move (king moves 2 squares to the left)
                moves.push_back(BitMove(square, coordsToSquare(2, 0), King));
            }
        }
    }
    // Black castling
    else if (!isWhite && !_blackKingMoved && y == 7 && x == 4) {
        // Kingside castling (e8 to g8)
        if (!_blackKingsideRookMoved) {
            // Check squares between king and rook are empty
            if (!isSquareOccupied(5, 7) && !isSquareOccupied(6, 7)) {
                // Add castling move
                moves.push_back(BitMove(square, coordsToSquare(6, 7), King));
            }
        }
        // Queenside castling (e8 to c8)
        if (!_blackQueensideRookMoved) {
            // Check squares between king and rook are empty
            if (!isSquareOccupied(1, 7) && !isSquareOccupied(2, 7) && !isSquareOccupied(3, 7)) {
                // Add castling move
                moves.push_back(BitMove(square, coordsToSquare(2, 7), King));
            }
        }
    }
}

// Evaluation function - returns score from current player's perspective
int Chess::evaluateBoard(const std::string& state, bool isWhite)
{
    int score = 0;

    // Material and positional evaluation
    for (int index = 0; index < 64; index++) {
        char piece = state[index];
        if (piece == '0') continue;

        bool pieceIsWhite = (piece >= 'A' && piece <= 'Z');
        char pieceChar = pieceIsWhite ? piece : (piece - 'a' + 'A');

        int materialValue = 0;
        int positionalValue = 0;

        // Material values based on piece type
        switch (pieceChar) {
            case 'P':
                materialValue = 100;
                positionalValue = pieceIsWhite ? pawnTable[index] : pawnTable[FLIP(index)];
                break;
            case 'N':
                materialValue = 320;
                positionalValue = pieceIsWhite ? knightTable[index] : knightTable[FLIP(index)];
                break;
            case 'B':
                materialValue = 330;
                positionalValue = pieceIsWhite ? bishopTable[index] : bishopTable[FLIP(index)];
                break;
            case 'R':
                materialValue = 500;
                positionalValue = pieceIsWhite ? rookTable[index] : rookTable[FLIP(index)];
                break;
            case 'Q':
                materialValue = 900;
                positionalValue = pieceIsWhite ? queenTable[index] : queenTable[FLIP(index)];
                break;
            case 'K':
                materialValue = 20000;
                positionalValue = pieceIsWhite ? kingTable[index] : kingTable[FLIP(index)];
                break;
            default:
                break;
        }

        int pieceValue = materialValue + positionalValue;

        // Add to score based on whose piece it is
        if (pieceIsWhite) {
            score += pieceValue;
        } else {
            score -= pieceValue;
        }
    }

    // Return from current player's perspective
    return isWhite ? score : -score;
}

/*
BoardState Chess::makeMove(const BitMove& move)
{
    BoardState state;
    state.fromSquare = move.from;
    state.toSquare = move.to;
    state.whiteKingMoved = _whiteKingMoved;
    state.blackKingMoved = _blackKingMoved;
    state.whiteKingsideRookMoved = _whiteKingsideRookMoved;
    state.whiteQueensideRookMoved = _whiteQueensideRookMoved;
    state.blackKingsideRookMoved = _blackKingsideRookMoved;
    state.blackQueensideRookMoved = _blackQueensideRookMoved;

    int fromX, fromY, toX, toY;
    squareToCoords(move.from, fromX, fromY);
    squareToCoords(move.to, toX, toY);

    ChessSquare* fromSquare = _grid->getSquare(fromX, fromY);
    ChessSquare* toSquare = _grid->getSquare(toX, toY);

    if (!fromSquare || !toSquare) {
        state.capturedPiece = nullptr;
        return state;
    }

    Bit* movingPiece = fromSquare->bit();
    state.capturedPiece = toSquare->bit();

    if (!movingPiece) {
        return state;
    }

    bool isWhite = isWhitePiece(movingPiece->gameTag());

    // Track castling rights
    if (move.piece == King) {
        if (isWhite) {
            _whiteKingMoved = true;

            // Check if this is a castling move
            if (fromX == 4 && toX == 6 && fromY == 0) {
                // White kingside castling - move the rook
                ChessSquare* rookFrom = _grid->getSquare(7, 0);
                ChessSquare* rookTo = _grid->getSquare(5, 0);
                if (rookFrom && rookTo && rookFrom->bit()) {
                    Bit* rook = rookFrom->bit();
                    rookFrom->setBitWithoutDelete(nullptr);
                    rookTo->setBitWithoutDelete(rook);
                    rook->setPosition(rookTo->getPosition());
                }
            } else if (fromX == 4 && toX == 2 && fromY == 0) {
                // White queenside castling - move the rook
                ChessSquare* rookFrom = _grid->getSquare(0, 0);
                ChessSquare* rookTo = _grid->getSquare(3, 0);
                if (rookFrom && rookTo && rookFrom->bit()) {
                    Bit* rook = rookFrom->bit();
                    rookFrom->setBitWithoutDelete(nullptr);
                    rookTo->setBitWithoutDelete(rook);
                    rook->setPosition(rookTo->getPosition());
                }
            }
        } else {
            _blackKingMoved = true;

            // Check if this is a castling move
            if (fromX == 4 && toX == 6 && fromY == 7) {
                // Black kingside castling - move the rook
                ChessSquare* rookFrom = _grid->getSquare(7, 7);
                ChessSquare* rookTo = _grid->getSquare(5, 7);
                if (rookFrom && rookTo && rookFrom->bit()) {
                    Bit* rook = rookFrom->bit();
                    rookFrom->setBitWithoutDelete(nullptr);
                    rookTo->setBitWithoutDelete(rook);
                    rook->setPosition(rookTo->getPosition());
                }
            } else if (fromX == 4 && toX == 2 && fromY == 7) {
                // Black queenside castling - move the rook
                ChessSquare* rookFrom = _grid->getSquare(0, 7);
                ChessSquare* rookTo = _grid->getSquare(3, 7);
                if (rookFrom && rookTo && rookFrom->bit()) {
                    Bit* rook = rookFrom->bit();
                    rookFrom->setBitWithoutDelete(nullptr);
                    rookTo->setBitWithoutDelete(rook);
                    rook->setPosition(rookTo->getPosition());
                }
            }
        }
    } else if (move.piece == Rook) {
        if (isWhite) {
            if (fromX == 7 && fromY == 0) _whiteKingsideRookMoved = true;
            if (fromX == 0 && fromY == 0) _whiteQueensideRookMoved = true;
        } else {
            if (fromX == 7 && fromY == 7) _blackKingsideRookMoved = true;
            if (fromX == 0 && fromY == 7) _blackQueensideRookMoved = true;
        }
    }

    // Move the piece - use setBitWithoutDelete to avoid deleting captured piece
    // (we save the captured piece in state to restore later)
    fromSquare->setBitWithoutDelete(nullptr);
    toSquare->setBitWithoutDelete(movingPiece);
    movingPiece->setPosition(toSquare->getPosition());

    return state;
}
*/

/*
void Chess::unmakeMove(const BitMove& move, const BoardState& state)
{
    int fromX, fromY, toX, toY;
    squareToCoords(state.fromSquare, fromX, fromY);
    squareToCoords(state.toSquare, toX, toY);

    ChessSquare* fromSquare = _grid->getSquare(fromX, fromY);
    ChessSquare* toSquare = _grid->getSquare(toX, toY);

    if (!fromSquare || !toSquare) return;

    Bit* movingPiece = toSquare->bit();
    if (!movingPiece) return;

    bool isWhite = isWhitePiece(movingPiece->gameTag());

    // Undo castling rook moves
    if (move.piece == King) {
        if (isWhite) {
            // White kingside castling
            if (fromX == 4 && toX == 6 && fromY == 0) {
                ChessSquare* rookFrom = _grid->getSquare(7, 0);
                ChessSquare* rookTo = _grid->getSquare(5, 0);
                if (rookFrom && rookTo && rookTo->bit()) {
                    Bit* rook = rookTo->bit();
                    rookTo->setBitWithoutDelete(nullptr);
                    rookFrom->setBitWithoutDelete(rook);
                    rook->setPosition(rookFrom->getPosition());
                }
            }
            // White queenside castling
            else if (fromX == 4 && toX == 2 && fromY == 0) {
                ChessSquare* rookFrom = _grid->getSquare(0, 0);
                ChessSquare* rookTo = _grid->getSquare(3, 0);
                if (rookFrom && rookTo && rookTo->bit()) {
                    Bit* rook = rookTo->bit();
                    rookTo->setBitWithoutDelete(nullptr);
                    rookFrom->setBitWithoutDelete(rook);
                    rook->setPosition(rookFrom->getPosition());
                }
            }
        } else {
            // Black kingside castling
            if (fromX == 4 && toX == 6 && fromY == 7) {
                ChessSquare* rookFrom = _grid->getSquare(7, 7);
                ChessSquare* rookTo = _grid->getSquare(5, 7);
                if (rookFrom && rookTo && rookTo->bit()) {
                    Bit* rook = rookTo->bit();
                    rookTo->setBitWithoutDelete(nullptr);
                    rookFrom->setBitWithoutDelete(rook);
                    rook->setPosition(rookFrom->getPosition());
                }
            }
            // Black queenside castling
            else if (fromX == 4 && toX == 2 && fromY == 7) {
                ChessSquare* rookFrom = _grid->getSquare(0, 7);
                ChessSquare* rookTo = _grid->getSquare(3, 7);
                if (rookFrom && rookTo && rookTo->bit()) {
                    Bit* rook = rookTo->bit();
                    rookTo->setBitWithoutDelete(nullptr);
                    rookFrom->setBitWithoutDelete(rook);
                    rook->setPosition(rookFrom->getPosition());
                }
            }
        }
    }

    // Move piece back - use setBitWithoutDelete
    toSquare->setBitWithoutDelete(state.capturedPiece);
    fromSquare->setBitWithoutDelete(movingPiece);
    movingPiece->setPosition(fromSquare->getPosition());

    // Restore castling rights
    _whiteKingMoved = state.whiteKingMoved;
    _blackKingMoved = state.blackKingMoved;
    _whiteKingsideRookMoved = state.whiteKingsideRookMoved;
    _whiteQueensideRookMoved = state.whiteQueensideRookMoved;
    _blackKingsideRookMoved = state.blackKingsideRookMoved;
    _blackQueensideRookMoved = state.blackQueensideRookMoved;
}
*/

// Negamax with alpha-beta pruning
int Chess::negamax(std::string& state, int depth, int alpha, int beta, bool isWhite)
{
    if (depth == 0) {
        return evaluateBoard(state, isWhite);
    }

    std::vector<BitMove> moves;
    generateMovesFromString(state, moves, isWhite);

    if (moves.empty()) {
        return evaluateBoard(state, isWhite);
    }

    int maxScore = std::numeric_limits<int>::min() + 1;

    for (const auto& move : moves) {
        // Save the piece at destination (for captures)
        char capturedPiece = state[move.to];

        state[move.to] = state[move.from];
        state[move.from] = '0';

        int score = -negamax(state, depth - 1, -beta, -alpha, !isWhite);

        state[move.from] = state[move.to];
        state[move.to] = capturedPiece;

        if (score > maxScore) {
            maxScore = score;
        }

        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            break;
        }
    }

    return maxScore;
}

// Get the best move for the current player
BitMove Chess::getBestMove()
{
    int currentPlayerNum = getCurrentPlayer()->playerNumber();
    bool isWhite = (currentPlayerNum == 0);

    std::vector<BitMove> moves;
    generateMoves(moves);

    if (moves.empty()) {
        return BitMove(); // No legal moves
    }

    BitMove bestMove = moves[0];
    int bestScore = std::numeric_limits<int>::min() + 1;
    int alpha = std::numeric_limits<int>::min() + 1;
    int beta = std::numeric_limits<int>::max();

    // Search at depth 3
    int searchDepth = 3;

    std::ofstream logFile("/tmp/chess_debug.log", std::ios::app);
    logFile << "\n=== AI SEARCHING at depth " << searchDepth << " ===" << std::endl;
    logFile << "Player " << (isWhite ? "WHITE" : "BLACK") << " evaluating " << moves.size() << " legal moves" << std::endl;

    // Get string copy of current board state
    std::string state = stateString();

    for (const auto& move : moves) {
        char capturedPiece = state[move.to];

        state[move.to] = state[move.from];
        state[move.from] = '0';

        int score = -negamax(state, searchDepth - 1, -beta, -alpha, !isWhite);

        state[move.from] = state[move.to];
        state[move.to] = capturedPiece;

        logFile << "Move from " << (int)move.from << " to " << (int)move.to
                << " score: " << score << std::endl;

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
            alpha = std::max(alpha, score);
        }
    }

    logFile << "Best move: from " << (int)bestMove.from << " to " << (int)bestMove.to
            << " with score " << bestScore << std::endl;
    logFile.close();

    return bestMove;
}

bool Chess::gameHasAI()
{
    return _gameOptions.AIPlaying;
}

void Chess::updateAI()
{
    if (!gameHasAI() || getCurrentPlayer()->playerNumber() != _gameOptions.AIPlayer) {
        return;
    }

    std::ofstream logFile("/tmp/chess_debug.log", std::ios::app);
    logFile << "\n=== AI MAKING MOVE ===" << std::endl;
    logFile << "Current player: " << getCurrentPlayer()->playerNumber() << std::endl;

    // Get the best move
    BitMove bestMove = getBestMove();

    if (bestMove.from == 0 && bestMove.to == 0 && bestMove.piece == NoPiece) {
        logFile << "No legal moves available" << std::endl;
        logFile.close();
        return;
    }

    // Execute the move
    int fromX, fromY, toX, toY;
    squareToCoords(bestMove.from, fromX, fromY);
    squareToCoords(bestMove.to, toX, toY);

    ChessSquare* fromSquare = _grid->getSquare(fromX, fromY);
    ChessSquare* toSquare = _grid->getSquare(toX, toY);

    if (!fromSquare || !toSquare || !fromSquare->bit()) {
        logFile << "Invalid move squares" << std::endl;
        logFile.close();
        return;
    }

    Bit* piece = fromSquare->bit();
    bool isWhite = isWhitePiece(piece->gameTag());

    logFile << "Moving piece from (" << fromX << "," << fromY << ") to ("
            << toX << "," << toY << ")" << std::endl;

    // Check if this is a castling move
    if (bestMove.piece == King) {
        if (isWhite && fromX == 4 && fromY == 0) {
            if (toX == 6) {
                logFile << "AI performing white kingside castling" << std::endl;
            } else if (toX == 2) {
                logFile << "AI performing white queenside castling" << std::endl;
            }
        } else if (!isWhite && fromX == 4 && fromY == 7) {
            if (toX == 6) {
                logFile << "AI performing black kingside castling" << std::endl;
            } else if (toX == 2) {
                logFile << "AI performing black queenside castling" << std::endl;
            }
        }
    }

    // Handle captured piece - delete it first
    Bit* capturedPiece = toSquare->bit();
    if (capturedPiece && capturedPiece != piece) {
        delete capturedPiece;
    }

    // Move the piece on the board - use setBitWithoutDelete since we already handled deletion
    fromSquare->setBitWithoutDelete(nullptr);
    toSquare->setBitWithoutDelete(piece);
    piece->setPosition(toSquare->getPosition());

    // Update castling rights and handle castling rook moves (call bitMovedFromTo for side effects)
    ChessPiece pieceType = getPieceType(piece->gameTag());

    if (pieceType == King) {
        if (isWhite) {
            _whiteKingMoved = true;

            // Handle castling rook movement
            if (fromX == 4 && toX == 6 && fromY == 0) {
                // White kingside castling - move rook
                ChessSquare* rookFrom = _grid->getSquare(7, 0);
                ChessSquare* rookTo = _grid->getSquare(5, 0);
                if (rookFrom && rookTo && rookFrom->bit()) {
                    Bit* rook = rookFrom->bit();
                    rookFrom->setBit(nullptr);
                    rookTo->setBit(rook);
                    rook->setPosition(rookTo->getPosition());
                }
            } else if (fromX == 4 && toX == 2 && fromY == 0) {
                // White queenside castling - move rook
                ChessSquare* rookFrom = _grid->getSquare(0, 0);
                ChessSquare* rookTo = _grid->getSquare(3, 0);
                if (rookFrom && rookTo && rookFrom->bit()) {
                    Bit* rook = rookFrom->bit();
                    rookFrom->setBit(nullptr);
                    rookTo->setBit(rook);
                    rook->setPosition(rookTo->getPosition());
                }
            }
        } else {
            _blackKingMoved = true;

            // Handle castling rook movement
            if (fromX == 4 && toX == 6 && fromY == 7) {
                // Black kingside castling - move rook
                ChessSquare* rookFrom = _grid->getSquare(7, 7);
                ChessSquare* rookTo = _grid->getSquare(5, 7);
                if (rookFrom && rookTo && rookFrom->bit()) {
                    Bit* rook = rookFrom->bit();
                    rookFrom->setBit(nullptr);
                    rookTo->setBit(rook);
                    rook->setPosition(rookTo->getPosition());
                }
            } else if (fromX == 4 && toX == 2 && fromY == 7) {
                // Black queenside castling - move rook
                ChessSquare* rookFrom = _grid->getSquare(0, 7);
                ChessSquare* rookTo = _grid->getSquare(3, 7);
                if (rookFrom && rookTo && rookFrom->bit()) {
                    Bit* rook = rookFrom->bit();
                    rookFrom->setBit(nullptr);
                    rookTo->setBit(rook);
                    rook->setPosition(rookTo->getPosition());
                }
            }
        }
    } else if (pieceType == Rook) {
        if (isWhite) {
            if (fromX == 7 && fromY == 0) _whiteKingsideRookMoved = true;
            if (fromX == 0 && fromY == 0) _whiteQueensideRookMoved = true;
        } else {
            if (fromX == 7 && fromY == 7) _blackKingsideRookMoved = true;
            if (fromX == 0 && fromY == 7) _blackQueensideRookMoved = true;
        }
    }

    // End the turn
    endTurn();

    logFile << "AI move completed and turn ended" << std::endl;
    logFile.close();
}
